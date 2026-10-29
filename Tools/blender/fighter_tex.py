"""Textures and materials for build_fighter.py.

Each material slot becomes one atlas written beside the FBX as
<id>_<slot>_basecolor.png (sRGB, RGBA for the alpha-tested face/hair slot), _normal.png
(DirectX / Unreal convention, green down), _orm.png (R occlusion, G roughness, B metallic) and,
where something glows, _emissive.png (sRGB).

* skin: MakeHuman's diffuse, tinted to the look colour, unused UV space (incl. the
  'MakeHuman.org / public domain' stamp) filled, optional bald-scalp clean-up, a mild detail
  normal from the diffuse, roughness from luminance.
* hairface / face: eyes, eyebrows, eyelashes and hair textures recoloured and packed into one
  atlas; the objects' UVs are remapped into their tiles.
* tiled slots (leather, cloth, metal, canvas, iron): parts packed into one UV atlas; each part
  samples its ambientCG set at a fixed world texel density, tinted to the look colour and
  rescaled to the look roughness.
"""
import math
import os
import re

import bpy
import numpy as np

import fighter_core as core
import fighter_geo as geo


# ----------------------------------------------------------------------------- colour

def hex_srgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4))


def srgb_to_lin(c):
    c = np.asarray(c, float)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def lin_to_srgb(c):
    c = np.clip(np.asarray(c, float), 0, 1)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)


def luminance(lin):
    return lin[..., 0] * 0.2126 + lin[..., 1] * 0.7152 + lin[..., 2] * 0.0722


def preview_material(name, rgb, rough=0.6, metal=0.0):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    p = mat.node_tree.nodes["Principled BSDF"]
    p.inputs["Base Color"].default_value = (*rgb, 1)
    p.inputs["Roughness"].default_value = rough
    p.inputs["Metallic"].default_value = metal
    return mat


def dev_materials(built):
    """Flat preview colours per surface (outfit stage only)."""
    cfg = built["ctx"].cfg
    flat = {"body": cfg["skin"]["color"], "eyes": "#e8e4dc",
            "brows": (cfg.get("brows") or {}).get("color", "#222222"),
            "lashes": (cfg.get("lashes") or {}).get("color", "#222222"),
            "hair": (cfg.get("hair") or {}).get("color", "#222222")}
    for ob in built["objects"].values():
        key = built["surface"][ob.name]
        sp = cfg["surfaces"].get(key, {})
        lin = tuple(srgb_to_lin(hex_srgb(sp.get("color", flat.get(key, "#ff00ff")))))
        ob.data.materials.clear()
        ob.data.materials.append(preview_material("dev_" + key, lin, sp.get("roughness", 0.6), sp.get("metallic", 0.0)))


# ----------------------------------------------------------------------------- image I/O

def load_rgba(path, size=None):
    img = bpy.data.images.load(path, check_existing=False)
    if size and tuple(img.size) != (size, size):
        img.scale(size, size)
    w, h = img.size
    a = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(a)
    bpy.data.images.remove(img)
    return a.reshape(h, w, 4)


def save_png(path, arr, alpha=False, colorspace="sRGB"):
    h, w = arr.shape[:2]
    if arr.shape[2] == 3:
        arr = np.concatenate([arr, np.ones((h, w, 1), arr.dtype)], 2)
    img = bpy.data.images.new(os.path.basename(path), w, h, alpha=alpha)
    img.colorspace_settings.name = colorspace
    img.pixels.foreach_set(np.ascontiguousarray(np.clip(arr, 0, 1), dtype=np.float32).ravel())
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)


def blur(a, r):
    """Separable box blur (edge-clamped), radius r pixels, applied twice (~gaussian)."""
    if r < 1:
        return a
    out = a.astype(np.float64)
    for _ in range(2):
        for ax in (0, 1):
            pad = [(0, 0)] * out.ndim
            pad[ax] = (r + 1, r)
            p = np.pad(out, pad, mode="edge")
            c = np.cumsum(p, axis=ax)
            hi = np.take(c, np.arange(2 * r + 1, c.shape[ax]), axis=ax)
            lo = np.take(c, np.arange(0, c.shape[ax] - 2 * r - 1), axis=ax)
            out = (hi - lo) / (2 * r + 1)
    return out


def sample(tex, u, v):
    """Bilinear, wrapping sample of an (h, w, c) texture at UV arrays."""
    h, w = tex.shape[:2]
    x = (np.mod(u, 1.0) * w) - 0.5
    y = (np.mod(v, 1.0) * h) - 0.5
    x0, y0 = np.floor(x).astype(int), np.floor(y).astype(int)
    fx, fy = (x - x0)[..., None], (y - y0)[..., None]
    x0, y0 = x0 % w, y0 % h
    x1, y1 = (x0 + 1) % w, (y0 + 1) % h
    return ((tex[y0, x0] * (1 - fx) + tex[y0, x1] * fx) * (1 - fy)
            + (tex[y1, x0] * (1 - fx) + tex[y1, x1] * fx) * fy)


# ----------------------------------------------------------------------------- rasterising

def uv_tris(ob):
    me = ob.data
    uvl = me.uv_layers.active.data
    uv = np.empty(len(me.loops) * 2)
    uvl.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    tris = []
    for p in me.polygons:
        ls = list(p.loop_indices)
        for k in range(1, len(ls) - 1):
            tris.append((uv[ls[0]], uv[ls[k]], uv[ls[k + 1]]))
    return np.array(tris).reshape(-1, 3, 2)


def raster(tris, size, idmap=None, value=0):
    """Fill texels covered by UV triangles with `value` in an int map (-1 = empty)."""
    if idmap is None:
        idmap = np.full((size, size), -1, np.int32)
    T = tris * size
    for t in T:
        x0, y0 = np.floor(t.min(0)).astype(int)
        x1, y1 = np.ceil(t.max(0)).astype(int)
        x0, y0 = max(x0, 0), max(y0, 0)
        x1, y1 = min(x1, size - 1), min(y1, size - 1)
        if x1 < x0 or y1 < y0:
            continue
        xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        (ax, ay), (bx, by), (cx, cy) = t
        d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(d) < 1e-12:
            continue
        w0 = ((by - cy) * (xs - cx) + (cx - bx) * (ys - cy)) / d
        w1 = ((cy - ay) * (xs - cx) + (ax - cx) * (ys - cy)) / d
        m = (w0 >= -1e-3) & (w1 >= -1e-3) & (1 - w0 - w1 >= -1e-3)
        sub = idmap[y0:y1 + 1, x0:x1 + 1]
        sub[m] = value
    return idmap


def dilate_ids(idmap, iters):
    out = idmap.copy()
    for _ in range(iters):
        empty = out < 0
        if not empty.any():
            break
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            sh = np.full_like(out, -1)
            ys = slice(max(dy, 0), out.shape[0] + min(dy, 0))
            yd = slice(max(-dy, 0), out.shape[0] + min(-dy, 0))
            xs = slice(max(dx, 0), out.shape[1] + min(dx, 0))
            xd = slice(max(-dx, 0), out.shape[1] + min(-dx, 0))
            sh[ys, xs] = out[yd, xd]
            fill = empty & (sh >= 0)
            out[fill] = sh[fill]
            empty &= ~fill
    return out


# ----------------------------------------------------------------------------- MakeHuman sources

def mhmat_texture(mhmat_path, key="diffuseTexture"):
    with open(mhmat_path) as f:
        for line in f:
            m = re.match(r"\s*%s\s+(\S+)" % key, line)
            if m:
                return os.path.join(os.path.dirname(mhmat_path), m.group(1))
    return None


def mhclo_material(mhclo_path):
    with open(mhclo_path) as f:
        for line in f:
            m = re.match(r"\s*material\s+(\S+)", line)
            if m:
                return os.path.normpath(os.path.join(os.path.dirname(mhclo_path), m.group(1)))
    return None


# ----------------------------------------------------------------------------- skin

def face_tris(ob, fmask):
    me = ob.data
    uv = np.empty(len(me.loops) * 2)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    tris = []
    for p, k in zip(me.polygons, fmask):
        if not k:
            continue
        ls = list(p.loop_indices)
        for j in range(1, len(ls) - 1):
            tris.append((uv[ls[0]], uv[ls[j]], uv[ls[j + 1]]))
    return np.array(tris).reshape(-1, 3, 2)


def skin_textures(fid, cfg, built, out_dir, size):
    body = built["objects"]["body"]
    sk = cfg["skin"]
    src = mhmat_texture(core.find_asset(sk["mhmat"] + ".mhmat", "skins"))
    rgba = load_rgba(src, size)
    lin = srgb_to_lin(rgba[..., :3])
    notes = []
    if sk.get("blend"):          # mix in an older MakeHuman skin (same UV layout) for lines and wear
        src2 = mhmat_texture(core.find_asset(sk["blend"]["mhmat"] + ".mhmat", "skins"))
        a2 = float(sk["blend"]["amount"])
        lin = lin * (1 - a2) + srgb_to_lin(load_rgba(src2, size)[..., :3]) * a2
        notes.append("blended %.0f%% of %s" % (a2 * 100, os.path.basename(src2)))
    cover = raster(uv_tris(body), size) >= 0
    target = srgb_to_lin(hex_srgb(sk["color"]))
    mean = lin[cover].mean(0)
    lin = np.clip(lin * (target / mean), 0, 1)
    notes.insert(0, "tinted %s -> %s (per-channel gain %s)" % (os.path.basename(src), sk["color"],
                                                               np.round(target / mean, 3).tolist()))
    if sk.get("flush") and "flush" in body.data.attributes:
        # weathering: cheeks, nose, ears and brow pushed towards a ruddy colour (mask from the build)
        fl = raster_scalar(body, attribute_values(body, "flush"), size)
        fl = blur(fl, 3)[..., None] * float(sk["flush"]["amount"])
        fc = srgb_to_lin(hex_srgb(sk["flush"]["color"]))
        L = luminance(lin)[..., None]
        lin = np.clip(lin * (1 - fl) + fc * (L / (luminance(target) + 1e-6)) * fl, 0, 1)
        notes.append("weathered flush %s on cheeks, nose, ears and brow" % sk["flush"]["color"])
    if sk.get("scalp_paint") and "hair" in built["objects"]:
        # short hair shows the scalp between its cards: paint the scalp under it in the hair colour
        lin = paint_scalp(cfg, built, body, lin, size)
        notes.append("scalp under the hair painted %s" % cfg["hair"]["color"])
    if sk.get("clean_scalp"):
        lin = clean_scalp(built, body, lin, size)
        notes.append("painted scalp hair replaced by forehead skin tone")
    # unused texels (deleted faces, MakeHuman watermark corner, unused islands) -> skin tone
    keep = dilate_ids(np.where(cover, 0, -1).astype(np.int32), 6) >= 0
    lin[~keep] = target
    notes.append("texels outside the body UVs (incl. the MakeHuman.org stamp) filled with %s" % sk["color"])
    emis = None
    if sk.get("veins"):
        lin, emis = paint_veins(lin, cover, sk["veins"], size)
        notes.append("procedural glowing veins %s" % sk["veins"]["color"])
    for ob in built["objects"].values():
        sp = cfg.get("surfaces", {}).get(built["surface"].get(ob.name), {})
        if sp.get("slot") == "skin" and sp.get("paint"):
            m = raster(uv_tris(ob), size) >= 0
            m = blur(m.astype(float), 2)[..., None]
            col = srgb_to_lin(hex_srgb(sp["color"]))
            rng = np.random.default_rng(7)
            n = rng.random((size, size))
            for _ in range(2):                    # streaks along v: hair strands, not blotches
                c_ = np.cumsum(np.pad(n, ((6, 6), (0, 0)), mode="edge"), axis=0)
                n = (c_[12:] - c_[:-12]) / 12.0
            n = (n - n.mean()) / (n.std() + 1e-9)
            strands = np.clip(1.0 + 0.35 * n, 0.45, 1.6)[..., None]
            lin = lin * (1 - m * 0.92) + col * strands * m * 0.92
            notes.append("%s painted %s under %s" % (sp.get("paint"), sp["color"], ob.name))
    base = lin_to_srgb(lin)
    L = luminance(lin)
    hp = L - blur(L, 6)
    h = blur(hp, 1)
    gy, gx = np.gradient(h)
    k = 6.0 * sk.get("detail", 1.0)
    n = np.stack([-gx * k * size / 2048, -gy * k * size / 2048, np.ones_like(h)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    n[..., 1] *= -1           # DirectX: green points down
    r0 = sk.get("roughness", 0.55)
    rough = np.clip(r0 + 0.6 * (blur(L, 3) - L) / (L.mean() + 1e-6), r0 - 0.17, r0 + 0.2)
    orm = np.stack([np.clip(1 - 2.0 * np.clip(blur(L, 2) - L, 0, 1), 0.6, 1), rough, np.zeros_like(L)], -1)
    return write_set(fid, "skin", out_dir, base, n * 0.5 + 0.5, orm, emis, alpha=None), notes


def attribute_values(ob, name):
    a = np.empty(len(ob.data.vertices), np.float32)
    ob.data.attributes[name].data.foreach_get("value", a)
    return a


def raster_scalar(ob, values, size):
    """Per-vertex values interpolated over the object's UV triangles into a size x size map."""
    me = ob.data
    uv = np.empty(len(me.loops) * 2)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2) * size
    lv = np.empty(len(me.loops), np.int64)
    me.loops.foreach_get("vertex_index", lv)
    out = np.zeros((size, size))
    for p in me.polygons:
        ls = list(p.loop_indices)
        vals = values[lv[ls]]
        if vals.max() <= 0:
            continue
        for k in range(1, len(ls) - 1):
            t = uv[[ls[0], ls[k], ls[k + 1]]]
            tv = vals[[0, k, k + 1]]
            x0, y0 = np.maximum(np.floor(t.min(0)).astype(int), 0)
            x1, y1 = np.minimum(np.ceil(t.max(0)).astype(int), size - 1)
            if x1 < x0 or y1 < y0:
                continue
            xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            (ax, ay), (bx, by), (cx, cy) = t
            d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
            if abs(d) < 1e-12:
                continue
            w0 = ((by - cy) * (xs - cx) + (cx - bx) * (ys - cy)) / d
            w1 = ((cy - ay) * (xs - cx) + (ax - cx) * (ys - cy)) / d
            w2 = 1 - w0 - w1
            m = (w0 >= -1e-3) & (w1 >= -1e-3) & (w2 >= -1e-3)
            sub = out[y0:y1 + 1, x0:x1 + 1]
            sub[m] = np.maximum(sub[m], (w0 * tv[0] + w1 * tv[1] + w2 * tv[2])[m])
    return out


def strand_noise(shape, rng, along=0, length=12):
    """Unit-variance noise smeared along one axis: hair strands rather than blotches."""
    n = rng.random(shape)
    for _ in range(2):
        pad = [(0, 0), (0, 0)]
        pad[along] = (length // 2, length // 2)
        c_ = np.cumsum(np.pad(n, pad, mode="edge"), axis=along)
        n = (np.take(c_, np.arange(length, c_.shape[along]), axis=along)
             - np.take(c_, np.arange(0, c_.shape[along] - length), axis=along)) / length
    return (n - n.mean()) / (n.std() + 1e-9)


def paint_scalp(cfg, built, body, lin, size):
    hair = built["objects"]["hair"]
    coll = geo.Collider([hair])
    V = geo.co(body)
    W = geo.weights(body)
    head = W.get("head", np.zeros(len(V)))
    d = np.array([coll.nearest(p)[2] or 1.0 for p in V])
    vm = (head > 0.5) & (d < 0.012)
    m = raster(face_tris(body, geo.face_mask(geo.faces(body), vm, "any")), size) >= 0
    m = blur(m.astype(float), 3)[..., None]
    col = srgb_to_lin(hex_srgb(cfg["hair"]["color"])) * 0.7
    strands = np.clip(1.0 + 0.35 * strand_noise((size, size), np.random.default_rng(11)), 0.45, 1.6)[..., None]
    return lin * (1 - m * 0.9) + col * strands * m * 0.9


def paint_veins(lin, cover, spec, size):
    """Branching glowing veins: thin ridges of a smooth multi-octave noise, darker green in the
    base colour and an emissive mask (sRGB)."""
    rng = np.random.default_rng(spec.get("seed", 3))
    acc = np.zeros((size, size))
    for k, w in ((48, 1.0), (24, 0.5), (12, 0.25)):
        acc += w * blur(rng.random((size, size)), max(1, size // k // 2))
    acc = (acc - acc.mean()) / (acc.std() + 1e-9)
    ridge = np.clip(1.0 - np.abs(acc) / spec.get("width", 0.06), 0, 1) ** 2
    gate = blur(rng.random((size, size)), max(2, size // 96))
    gate = (gate - gate.mean()) / (gate.std() + 1e-9)
    detail = np.clip(gate * 1.6 - 0.2, 0, 1)          # breaks the contour loops into branches
    veins = ridge * detail * cover
    col = srgb_to_lin(hex_srgb(spec["color"]))
    lin = lin * (1 - 0.6 * veins[..., None]) + col * 0.35 * veins[..., None]
    emis = lin_to_srgb(col * spec.get("emission", 1.0) * veins[..., None])
    return lin, emis


def clean_scalp(built, body, lin, size):
    """Bald heads: MakeHuman skins have painted short hair; repaint the scalp with the forehead
    tone (keeping a little pore detail) with a feathered edge."""
    ctx = built["ctx"]
    V = geo.co(body)
    W = geo.weights(body, ["head"])
    FV = geo.faces(body)
    brows = built["objects"].get("brows")
    zb = float(geo.co(brows)[:, 2].mean()) if brows is not None else float(V[W["head"] > 0.9, 2].mean())
    head = W["head"] > 0.9
    yf = V[head, 1].min()
    yb = V[head, 1].max()
    s = ctx.s
    # hairline plane: 5.5 cm above the brows at the forehead, falling to brow level - 6 cm at the nape
    zline = zb + 0.055 * s + (V[:, 1] - yf) / (yb - yf) * (-0.17 * s)
    scalp_v = head & (V[:, 2] > zline)
    fore_v = head & (V[:, 2] > zb + 0.012 * s) & (V[:, 2] < zb + 0.04 * s) & (V[:, 1] < yf + 0.03 * s)
    scalp = raster(face_tris(body, geo.face_mask(FV, scalp_v, "mean")), size) >= 0
    fore = raster(face_tris(body, geo.face_mask(FV, fore_v, "all")), size) >= 0
    tone = lin[fore].mean(0) if fore.any() else lin.mean((0, 1))
    m = np.clip(blur(scalp.astype(float), 9) * 1.6, 0, 1)[..., None]
    L = luminance(lin)
    detail = (L - blur(L, 4)) / (blur(L, 4) + 1e-4)
    fill = tone * (1 + 0.25 * np.clip(detail, -0.3, 0.3))[..., None]
    mot = cfg_skin_mottle(built)
    if mot:
        # old skin: soft blotches and a scatter of small age spots (opt-in, 'scalp_mottle' in the skin spec)
        size = lin.shape[0]
        rng = np.random.default_rng(31)
        n1 = blur(rng.random((size, size)), max(2, size // 90))
        n1 = (n1 - n1.mean()) / (n1.std() + 1e-9)
        spots = np.clip(blur((rng.random((size, size)) > 0.99985).astype(float), max(1, size // 500)) * 25, 0, 1)
        brown = np.array([0.62, 0.5, 0.38])
        fill = fill * (1 + mot * 0.3 * np.clip(n1, -2, 2))[..., None]
        k = np.clip(spots * mot, 0, 0.6)[..., None]
        fill = fill * (1 - k) + tone * brown * k
    return lin * (1 - m) + fill * m


def cfg_skin_mottle(built):
    return float(built["ctx"].cfg["skin"].get("scalp_mottle", 0.0))


# ----------------------------------------------------------------------------- face / hair atlas

def recolour(lin, target, gamma=1.0, alpha=None):
    """Recolour keeping the luminance pattern; the median texel (of the opaque ones when an
    alpha is given) lands on the target colour."""
    L = luminance(lin)
    ref = (L > 0.02) if alpha is None else (alpha > 0.5)
    mL = float(np.median(L[ref])) if ref.any() else 0.5
    return np.clip(np.asarray(target)[None, None, :] * ((np.maximum(L, 1e-4) / mL) ** gamma)[..., None], 0, 1)


def face_atlas(fid, cfg, built, out_dir, slot, size):
    objs = built["objects"]
    has_hair = "hair" in objs
    if has_hair:
        tiles = {"hair": (0.0, 0.5, 1.0, 0.5), "eyes": (0.0, 0.0, 0.5, 0.5),
                 "brows": (0.5, 0.25, 0.25, 0.25), "lashes": (0.75, 0.25, 0.25, 0.25)}
    else:
        tiles = {"eyes": (0.0, 0.0, 0.5, 0.5), "brows": (0.5, 0.0, 0.5, 0.5), "lashes": (0.0, 0.5, 0.5, 0.5)}
    base = np.zeros((size, size, 4))
    orm = np.zeros((size, size, 3))
    orm[..., 0] = 1.0
    orm[..., 1] = 0.6
    emis = np.zeros((size, size, 3))
    srcs = {}
    eyes_mhmat = core.find_asset("materials/%s.mhmat" % cfg["eyes"]["mhmat"], "eyes")
    srcs["eyes"] = mhmat_texture(eyes_mhmat)
    for key, sub in (("brows", "eyebrows"), ("lashes", "eyelashes"), ("hair", "hair")):
        if key in objs and cfg.get(key):
            mhclo = core.find_asset(cfg[key]["asset"] + ".mhclo", sub)
            srcs[key] = mhmat_texture(mhclo_material(mhclo))
    notes = []
    for key, (x0, y0, w, h) in tiles.items():
        if key not in objs:
            continue
        pw, ph = int(round(w * size)), int(round(h * size))
        img = bpy.data.images.load(srcs[key], check_existing=False)
        img.scale(pw, ph)
        a = np.empty(pw * ph * 4, np.float32)
        img.pixels.foreach_get(a)
        bpy.data.images.remove(img)
        a = a.reshape(ph, pw, 4)
        lin = srgb_to_lin(a[..., :3])
        rough = 0.6
        if key == "eyes":
            lin, iris = tint_iris(lin, cfg["eyes"])
            rough = 0.12
            if cfg["eyes"].get("emission"):
                e = srgb_to_lin(hex_srgb(cfg["eyes"].get("glow", cfg["eyes"]["iris"]))) * cfg["eyes"]["emission"]
                emis[int(y0 * size):int(y0 * size) + ph, int(x0 * size):int(x0 * size) + pw] = \
                    lin_to_srgb(e * iris[..., None])
        else:
            col = srgb_to_lin(hex_srgb(cfg[key]["color"]))
            lin = recolour(lin, col, cfg[key].get("contrast", 0.9 if key == "hair" else 0.6),
                           alpha=a[..., 3] if cfg[key].get("match") == "opaque" else None)
            rough = cfg[key].get("roughness", 0.45 if key == "hair" else 0.7)
        Y, X = int(y0 * size), int(x0 * size)
        base[Y:Y + ph, X:X + pw, :3] = lin_to_srgb(lin)
        base[Y:Y + ph, X:X + pw, 3] = a[..., 3]
        orm[Y:Y + ph, X:X + pw, 1] = rough
        notes.append("%s: %s -> tile %s" % (key, os.path.relpath(srcs[key], core.PROJECT), (x0, y0, w, h)))
        remap_uv(objs[key], x0, y0, w, h)
    if cfg.get("beard"):
        # with hair the beard shares the lower right quarter; a bald head has the whole upper right quarter
        bt = ((0.5, 0.0, 0.25, 0.25), (0.75, 0.0, 0.25, 0.25)) if has_hair else ((0.5, 0.5, 0.25, 0.5), (0.75, 0.5, 0.25, 0.5))
        for key, tile, dense in (("beard", bt[0], True), ("beard_outer", bt[1], False)):
            if key not in objs:
                continue
            x0, y0, w, h = tile
            remap_uv(objs[key], x0, y0, w, h)
            X, Y, pw, ph = int(x0 * size), int(y0 * size), int(w * size), int(h * size)
            cov = (raster(uv_tris(objs[key]), size) >= 0)[Y:Y + ph, X:X + pw]
            mu, mv = (np.asarray(objs[key]["uv_span_m"], float) / [pw, ph])
            rgb, alpha = fur_tile(cfg["beard"], cov, mu, mv, dense, seed=5 if dense else 9)
            base[Y:Y + ph, X:X + pw, :3] = lin_to_srgb(rgb)
            base[Y:Y + ph, X:X + pw, 3] = alpha
            orm[Y:Y + ph, X:X + pw, 1] = cfg["beard"].get("roughness", 0.85)
            notes.append("%s: generated fur strands %s, alpha edges -> tile %s" % (key, cfg["beard"]["color"], tile))
    if cfg.get("braids"):
        # procedural tight hair (no MakeHuman hair): a cap pulled back over the scalp and braid lobes,
        # in the free upper right quarter of a hairless atlas
        for key, tile, lobes in (("hair_cap", (0.5, 0.5, 0.25, 0.5), False), ("braids", (0.75, 0.5, 0.25, 0.5), True)):
            if key not in objs:
                continue
            x0, y0, w, h = tile
            remap_uv(objs[key], x0, y0, w, h)
            X, Y, pw, ph = int(x0 * size), int(y0 * size), int(w * size), int(h * size)
            cov = (raster(uv_tris(objs[key]), size) >= 0)[Y:Y + ph, X:X + pw]
            mu, mv = (np.asarray(objs[key]["uv_span_m"], float) / [pw, ph])
            rgb, alpha = hair_tile(cfg["braids"], cov, mu, mv, lobes, seed=21 if lobes else 17)
            base[Y:Y + ph, X:X + pw, :3] = lin_to_srgb(rgb)
            base[Y:Y + ph, X:X + pw, 3] = alpha
            orm[Y:Y + ph, X:X + pw, 1] = cfg["braids"].get("roughness", 0.55)
            notes.append("%s: generated hair strands %s -> tile %s" % (key, cfg["braids"]["color"], tile))
    normal = np.zeros((size, size, 3)) + [0.5, 0.5, 1.0]
    files = write_set(fid, slot, out_dir, base, normal, orm, emis if emis.any() else None, alpha=True)
    return files, notes


def hair_tile(spec, cov, mu, mv, lobes, seed):
    """Tight hair: strands along v (the cap's front-to-back direction, or a braid lobe's length),
    lighter and darker strands, a soft ragged hairline (cap) or ragged lobe tips (braids). Lobes
    share one UV rectangle, so only their v ends fade."""
    ph, pw = cov.shape
    rng = np.random.default_rng(seed)
    col = srgb_to_lin(hex_srgb(spec["color"]))
    lite = srgb_to_lin(hex_srgb(spec.get("highlight", spec["color"])))
    L = 2 * max(4, int(0.015 / mv))                     # (strand_noise needs an even length)
    n1 = strand_noise((ph, pw), rng, along=0, length=L)
    n2 = strand_noise((ph, pw), rng, along=0, length=2 * max(2, L // 6))
    k = np.clip(0.5 + 0.22 * n1 + 0.12 * n2, 0, 1)[..., None]
    rgb = col * (0.62 + 0.55 * k) * (1 - 0.35 * k) + lite * 0.35 * k * k
    if lobes:
        vv = (np.arange(ph)[:, None] + 0.5) / ph * np.ones((1, pw))
        tip = np.minimum(vv, 1 - vv) * ph * mv                       # metres from the lobe tips
        edge = np.clip(tip / 0.004, 0, 1)
        shade = 0.72 + 0.28 * np.sin(np.pi * np.clip(vv, 0, 1)) ** 0.6   # darker where lobes tuck under
        rgb = rgb * shade[..., None]
        strands = np.clip(0.5 + 0.5 * n2, 0, 1)
        alpha = np.maximum(edge ** 2, strands * (tip > 0.0007) * (edge > 0.2))
        alpha = np.where(edge >= 1, 1.0, alpha)
    else:
        cov = cov.copy()
        cov[:, :3] |= cov[:, 3:4]          # u = 0 is the parting line between mirrored halves, not an edge
        d = edge_distance(cov, mu, mv, max_m=0.01)
        edge = np.clip(d / 0.006, 0, 1)
        strands = np.clip(0.55 + 0.6 * n2, 0, 1)
        alpha = np.where(edge >= 1, 1.0, np.maximum(edge ** 3, strands * edge * (d > 0.0005)))
        alpha = np.where(cov, alpha, 0.0)
    return np.clip(rgb, 0, 1), np.clip(alpha, 0, 1)


def edge_distance(cov, mu, mv, max_m=0.012):
    """Approximate distance (metres) from each covered texel to the edge of the coverage, for
    texels mu x mv metres (4-neighbour relaxation)."""
    d = np.where(cov, np.inf, 0.0)
    steps = int(max_m / min(mu, mv)) + 2
    for _ in range(steps):
        nd = d.copy()
        nd[1:, :] = np.minimum(nd[1:, :], d[:-1, :] + mv)
        nd[:-1, :] = np.minimum(nd[:-1, :], d[1:, :] + mv)
        nd[:, 1:] = np.minimum(nd[:, 1:], d[:, :-1] + mu)
        nd[:, :-1] = np.minimum(nd[:, :-1], d[:, 1:] + mu)
        d = nd
    d[~cov] = 0.0
    return np.minimum(d, max_m)


def fur_tile(spec, cov, mu, mv, dense, seed):
    """Beard fur for one shell: strands 0.9 mm wide and 6-14 mm long hanging down the tile
    (v is height on the face), some grey, darker between them. The dense core layer is opaque
    6 mm inside its outline and breaks into single strands towards it; the outer layer is
    strands only. Alpha fades out at the outline, so the shell's edge is a ragged fur edge."""
    ph, pw = cov.shape
    rng = np.random.default_rng(seed)
    d = edge_distance(cov, mu, mv)
    col = srgb_to_lin(hex_srgb(spec["color"]))
    grey = srgb_to_lin(hex_srgb("#857e75"))
    area = cov.sum() * mu * mv
    n = int(area / (0.0009 * 0.010) * (1.6 if dense else 0.9) * spec.get("density", 1.0))
    x0 = rng.random(n) * pw
    y0 = rng.random(n) * ph
    L = np.maximum((rng.uniform(0.006, 0.014, n) / mv).astype(int), 2)
    centre = np.abs(x0 - pw / 2) * mu < 0.04          # the moustache: strands fan outwards
    slant = (rng.normal(0, 0.18, n) + np.where(centre, 0.35 * np.sign(x0 - pw / 2), 0.0)) * mv / mu
    val = rng.uniform(0.7, 1.3, n)
    gr = rng.random(n) < 0.08
    idx = np.repeat(np.arange(n), L)
    t = np.arange(idx.size) - np.repeat(np.cumsum(L) - L, L)
    ys = (y0[idx] - t).astype(int)
    xs = x0[idx] + slant[idx] * t
    ok = (ys >= 0) & (ys < ph) & (xs >= 0) & (xs < pw - 1)
    ys, xs, idx, t = ys[ok], xs[ok], idx[ok], t[ok]
    xi = xs.astype(int)
    fade = 1.0 - 0.45 * (t / L[idx]) ** 2
    a_str = np.zeros((ph, pw))
    np.maximum.at(a_str, (ys, xi), fade)
    np.maximum.at(a_str, (ys, xi + 1), fade * 0.6)
    v_map = np.full((ph, pw), 0.0)
    g_map = np.zeros((ph, pw))
    v_map[ys, xi] = val[idx] * fade
    g_map[ys, xi] = gr[idx]
    edge = np.clip(d / ((spec.get("core_edge", 0.006) if dense else 0.004)), 0, 1)
    edge = edge * edge * (3 - 2 * edge)
    strand_col = (col[None, None, :] * v_map[..., None] * (1 - g_map[..., None] * 0.8)
                  + grey[None, None, :] * g_map[..., None] * 0.8)
    if dense:
        under = col[None, None, :] * 0.7
        rgb = under * (1 - a_str[..., None]) + strand_col * a_str[..., None]
        alpha = np.maximum(edge, a_str * (d > 0.0004))
    else:
        rgb = col[None, None, :] * 0.8 * (1 - a_str[..., None]) + strand_col * a_str[..., None]
        alpha = a_str * edge
    alpha = np.where(cov, np.clip(alpha, 0, 1), 0.0)
    return np.clip(rgb, 0, 1), alpha


def tint_iris(lin, eyes):
    """Push the iris of a MakeHuman eye texture towards the look colour (pupil kept)."""
    h, w = lin.shape[:2]
    target = srgb_to_lin(hex_srgb(eyes["iris"]))
    ys, xs = np.mgrid[0:h, 0:w]
    # the two irises sit at (0.705, 0.70) and (0.29, 0.295) of the MakeHuman eye texture (bottom-up rows)
    iris = np.zeros((h, w))
    for cx, cy in ((0.705, 0.70), (0.293, 0.297)):
        r = np.hypot(xs / w - cx, ys / h - cy) / 0.115
        iris = np.maximum(iris, np.clip((1.0 - r) / 0.12, 0, 1))
    L = luminance(lin)
    pupil = np.clip((0.04 - L) / 0.03, 0, 1)
    mL = float(np.median(L[iris > 0.5])) if (iris > 0.5).any() else 0.2
    col = np.clip(np.asarray(target)[None, None, :] * ((np.maximum(L, 1e-4) / mL) ** 0.8)[..., None], 0, 1)
    m = (iris * (1 - pupil))[..., None] * 0.85
    return lin * (1 - m) + col * m, iris * (1 - pupil)


def remap_uv(ob, x0, y0, w, h):
    uvl = ob.data.uv_layers.active.data
    uv = np.empty(len(uvl) * 2)
    uvl.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    uv = np.clip(uv, 0.0005, 0.9995)
    uv[:, 0] = x0 + uv[:, 0] * w
    uv[:, 1] = y0 + uv[:, 1] * h
    uvl.foreach_set("uv", uv.ravel())


# ----------------------------------------------------------------------------- tiled slots

def ensure_uvs(ob):
    if ob.data.uv_layers:
        return
    ob.data.uv_layers.new(name="UVMap")
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.01, scale_to_bounds=False)
    bpy.ops.object.mode_set(mode="OBJECT")


def pack_slot(objs, margin=0.006):
    for k, ob in enumerate(objs):
        ensure_uvs(ob)
        for i, l in enumerate(list(ob.data.uv_layers)):
            if i > 0:
                ob.data.uv_layers.remove(l)
        ob.data.uv_layers.active_index = 0
        # move each object's UVs to its own region first: overlapping islands get merged by
        # the packer (wanted for a shell's inner/outer copies, not between different parts)
        uvl = ob.data.uv_layers.active.data
        uv = np.empty(len(uvl) * 2)
        uvl.foreach_get("uv", uv)
        uv = uv.reshape(-1, 2)
        uv = (uv - uv.min(0)) / max(float((uv.max(0) - uv.min(0)).max()), 1e-9)
        uv[:, 0] += 1.5 * k
        uvl.foreach_set("uv", uv.ravel())
    bpy.ops.object.select_all(action="DESELECT")
    for ob in objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.select_all(action="SELECT")
    bpy.ops.uv.average_islands_scale()
    bpy.ops.uv.pack_islands(rotate=True, margin=margin, merge_overlap=True)
    bpy.ops.object.mode_set(mode="OBJECT")


def area3d_uv(ob):
    me = ob.data
    c = geo.co(ob)
    uv = np.empty(len(me.loops) * 2)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    a3 = auv = 0.0
    for p in me.polygons:
        vs = list(p.vertices)
        ls = list(p.loop_indices)
        for k in range(1, len(vs) - 1):
            a3 += 0.5 * np.linalg.norm(np.cross(c[vs[k]] - c[vs[0]], c[vs[k + 1]] - c[vs[0]]))
            e1, e2 = uv[ls[k]] - uv[ls[0]], uv[ls[k + 1]] - uv[ls[0]]
            auv += 0.5 * abs(e1[0] * e2[1] - e1[1] * e2[0])
    return a3, auv


_SETS = {}


def acg_maps(set_id):
    if set_id in _SETS:
        return _SETS[set_id]
    d = os.path.join(core.ACG_ROOT, set_id, set_id + "_1K-JPG")
    maps = {}
    for key, fn in (("color", "Color"), ("normal", "NormalDX"), ("rough", "Roughness"),
                    ("ao", "AmbientOcclusion"), ("metal", "Metalness")):
        p = os.path.join(d, "%s_1K-JPG_%s.jpg" % (set_id, fn))
        if os.path.isfile(p):
            maps[key] = load_rgba(p)[..., :3]
    c = srgb_to_lin(maps["color"])
    maps["lum"] = luminance(c)
    maps["lum_mean"] = float(maps["lum"].mean())
    maps["rough_mean"] = float(maps["rough"][..., 0].mean()) if "rough" in maps else 0.5
    _SETS[set_id] = maps
    return maps


def tiled_slot(fid, cfg, built, out_dir, slot, objs, size):
    pack_slot(objs)
    idmap = np.full((size, size), -1, np.int32)
    specs, scale = [], []
    for i, ob in enumerate(objs):
        raster(uv_tris(ob), size, idmap, i)
        sp = cfg["surfaces"][built["surface"][ob.name]]
        a3, auv = area3d_uv(ob)
        specs.append(sp)
        scale.append(math.sqrt(a3 / max(auv, 1e-12)))
    idmap = dilate_ids(idmap, 12)
    idmap[idmap < 0] = 0
    ys, xs = np.mgrid[0:size, 0:size]
    u, v = (xs + 0.5) / size, (ys + 0.5) / size
    base = np.zeros((size, size, 3))
    normal = np.zeros((size, size, 3)) + [0.5, 0.5, 1.0]
    orm = np.zeros((size, size, 3))
    emis = np.zeros((size, size, 3))
    alpha = np.ones((size, size))
    any_alpha = False
    notes = []
    for i, (sp, mpu) in enumerate(zip(specs, scale)):
        m = idmap == i
        if not m.any():
            continue
        if sp["set"] == "flat" or sp["set"].startswith("procedural:"):
            target = srgb_to_lin(hex_srgb(sp["color"]))
            base[m] = lin_to_srgb(np.broadcast_to(target, (m.sum(), 3)))
            orm[m] = np.stack([np.ones(m.sum()), np.full(m.sum(), sp["roughness"]),
                               np.full(m.sum(), sp["metallic"])], 1)
            if sp["set"] in ("procedural:lace", "procedural:lace_dense"):
                k = mpu / sp["tile_m"]
                fn = lace_alpha if sp["set"] == "procedural:lace" else lace_dense_alpha
                alpha[m] = fn(u[m] * k, v[m] * k)
                any_alpha = True
            if sp.get("emission"):
                emis[m] = lin_to_srgb(target * sp["emission"])
            notes.append("%s: flat %s%s" % (objs[i].name, sp["color"], " + procedural lace" if "lace" in sp["set"] else ""))
            continue
        mp = acg_maps(sp["set"])
        k = mpu / sp["tile_m"]
        uu, vv = u[m] * k, v[m] * k
        col = srgb_to_lin(sample(mp["color"], uu, vv))
        L = luminance(col)
        g = sp.get("contrast", 0.55 if sp["metallic"] > 0.5 else 0.85)   # how much of the set's pattern shows
        target = srgb_to_lin(hex_srgb(sp["color"]))
        base[m] = lin_to_srgb(target * (np.maximum(L, 1e-4) / mp["lum_mean"])[:, None] ** g)
        if "normal" in mp and sp.get("normal", 1.0) > 0:
            n = sample(mp["normal"], uu, vv) * 2 - 1
            n[:, :2] *= sp["normal"]
            n /= np.linalg.norm(n, axis=1, keepdims=True)
            normal[m] = n * 0.5 + 0.5
        r = sample(mp["rough"], uu, vv)[:, 0] if "rough" in mp else np.full(m.sum(), mp["rough_mean"])
        r = np.clip(r * sp["roughness"] / mp["rough_mean"], 0.05, 1.0)
        ao = sample(mp["ao"], uu, vv)[:, 0] if "ao" in mp else np.ones(m.sum())
        orm[m] = np.stack([0.35 + 0.65 * ao, r, np.full(m.sum(), sp["metallic"])], 1)
        if sp.get("emission"):
            emis[m] = lin_to_srgb(target * sp["emission"])
        if sp.get("pattern"):
            n_emb = embroider(objs[i], sp["pattern"], size, m, base, orm, (np.maximum(L, 1e-4) / mp["lum_mean"]) ** g)
            notes.append("%s: %s embroidery in %s on %d texels" % (objs[i].name, sp["pattern"]["kind"], sp["pattern"]["thread"], n_emb))
        notes.append("%s: %s tinted %s, %.0f cm tiles, roughness %.2f, metallic %.0f" % (
            objs[i].name, sp["set"], sp["color"], sp["tile_m"] * 100, sp["roughness"], sp["metallic"]))
    if any_alpha:
        base = np.concatenate([base, alpha[..., None]], -1)
    files = write_set(fid, slot, out_dir, base, normal, orm, emis if emis.any() else None,
                      alpha=True if any_alpha else None)
    return files, notes, any_alpha


# ----------------------------------------------------------------------------- embroidery

def raster_attrs(ob, names, size, period=None):
    """Per-vertex float attributes interpolated over the object's UV triangles into (size, size, k)
    maps (NaN where the object has no texels). `period` unwraps the first attribute across a seam
    (an along-the-band coordinate that runs round a closed ring)."""
    me = ob.data
    uv = np.empty(len(me.loops) * 2)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2) * size
    lv = np.empty(len(me.loops), np.int64)
    me.loops.foreach_get("vertex_index", lv)
    vals = np.stack([attribute_values(ob, n) for n in names], 1).astype(float)
    out = np.full((size, size, len(names)), np.nan)
    for p in me.polygons:
        ls = list(p.loop_indices)
        pv = vals[lv[ls]].copy()
        if (pv[:, 1] < -0.3).all() or (pv[:, 1] > 1.3).all():
            continue                      # nowhere near the band
        if period:
            pv[:, 0] = pv[0, 0] + np.mod(pv[:, 0] - pv[0, 0] + period / 2, period) - period / 2
        for k in range(1, len(ls) - 1):
            t = uv[[ls[0], ls[k], ls[k + 1]]]
            tv = pv[[0, k, k + 1]]
            x0, y0 = np.maximum(np.floor(t.min(0)).astype(int), 0)
            x1, y1 = np.minimum(np.ceil(t.max(0)).astype(int), size - 1)
            if x1 < x0 or y1 < y0:
                continue
            xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            (ax, ay), (bx, by), (cx, cy) = t
            d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
            if abs(d) < 1e-12:
                continue
            w0 = ((by - cy) * (xs - cx) + (cx - bx) * (ys - cy)) / d
            w1 = ((cy - ay) * (xs - cx) + (ax - cx) * (ys - cy)) / d
            w2 = 1 - w0 - w1
            m = (w0 >= -1e-3) & (w1 >= -1e-3) & (w2 >= -1e-3)
            sub = out[y0:y1 + 1, x0:x1 + 1]
            sub[m] = (w0[..., None] * tv[0] + w1[..., None] * tv[1] + w2[..., None] * tv[2])[m]
    return out


def cloud_band(u, v):
    """Pale-gold embroidery for a robe's edge band: a double border and a running scroll of ruyi
    clouds (a big curl rolling in, an inner turn, two small side curls on a flat base), every
    other one upside down, joined by a wave through the band's centre line. u runs along the
    band in band widths, v across it (0..1). Returns stroke coverage and a dark halo round the
    strokes (the shadow of raised thread)."""
    u = np.asarray(u, float)
    v = np.asarray(v, float)
    P = 1.7
    k = np.floor(u / P)
    x = u - (k + 0.5) * P
    y = (v - 0.5) * np.where(np.mod(k, 2) == 0, 1.0, -1.0)
    d = np.minimum(np.abs(v - 0.09), np.abs(v - 0.91))
    d = np.minimum(d, np.minimum(np.abs(v - 0.155), np.abs(v - 0.845)) + 0.012)

    def arc(cx, cy, r, a0, a1):
        ang = np.arctan2(y - cy, x - cx)
        t = np.mod(ang - a0, 2 * math.pi)
        e0 = np.hypot(x - cx - r * math.cos(a0), y - cy - r * math.sin(a0))
        e1 = np.hypot(x - cx - r * math.cos(a1), y - cy - r * math.sin(a1))
        return np.where(t <= (a1 - a0) % (2 * math.pi), np.abs(np.hypot(x - cx, y - cy) - r), np.minimum(e0, e1))

    c = arc(0.0, 0.03, 0.17, -0.2, 3.4)
    c = np.minimum(c, arc(0.035, 0.05, 0.085, 0.6, 4.6))
    c = np.minimum(c, arc(-0.27, -0.08, 0.09, -0.4, 3.0))
    c = np.minimum(c, arc(-0.27, -0.08, 0.04, 0.8, 4.2))
    c = np.minimum(c, arc(0.27, -0.08, 0.09, 0.15, 3.55))
    c = np.minimum(c, arc(0.27, -0.08, 0.04, -1.1, 2.3))
    c = np.minimum(c, np.where(np.abs(x) <= 0.36, np.abs(y + 0.17), 9.0))
    xx = np.abs(x)
    tw = np.clip((xx - 0.36) / (P / 2 - 0.36), 0, 1)
    wave = -0.17 * (1 + np.cos(math.pi * tw)) / 2
    c = np.minimum(c, np.where(xx > 0.36, np.abs(y - wave) * 0.9, 9.0))
    d = np.minimum(d, c)
    w, aa = 0.022, 0.01
    a = np.clip((w - d) / aa + 0.5, 0, 1)
    halo = np.clip(np.clip((w + 0.03 - d) / 0.02, 0, 1) - a, 0, 1)
    return a, halo


def stitch_band(u, v):
    """Rows of machine stitching along a belt: five parallel lines pressed into the cloth."""
    v = np.asarray(v, float)
    d = np.min([np.abs(v - r) for r in (0.13, 0.32, 0.5, 0.68, 0.87)], axis=0)
    a = np.clip((0.022 - d) / 0.014 + 0.5, 0, 1)
    halo = np.clip(np.clip((0.05 - d) / 0.02, 0, 1) - a, 0, 1)
    return a * 0.8, halo


def panel_band(u, v):
    """A reinforcement panel (the part's band colour fills v < 1) with a dashed stitched seam
    just inside its edge."""
    v = np.asarray(v, float)
    d = np.abs(v - 0.86)
    on = np.mod(np.asarray(u, float) * 2.0, 1.0) < 0.62
    a = np.clip((0.028 - d) / 0.016 + 0.5, 0, 1) * on
    halo = np.clip(np.clip((0.06 - d) / 0.025, 0, 1) - a, 0, 1)
    return a * 0.9, halo


PATTERNS = {"cloud": cloud_band, "stitch": stitch_band, "panel": panel_band}


def embroider(ob, pat, size, m, base, orm, lum):
    """Edge band with embroidery on the texels `m` of one part: the part carries per-vertex
    'emb_u' (along the band, in band widths) and 'emb_v' (across it, 0..1; outside that range the
    part keeps its own cloth). pat: kind, thread colour, optional band colour (else the part's)."""
    if "emb_u" not in ob.data.attributes:
        return 0
    A = raster_attrs(ob, ("emb_u", "emb_v"), size, ob.get("emb_period"))
    eu, ev = A[..., 0][m], A[..., 1][m]
    inb = np.isfinite(ev) & (ev >= 0) & (ev <= 1)
    if not inb.any():
        return 0
    cur = srgb_to_lin(base[m])
    if pat.get("band"):
        cur[inb] = np.asarray(srgb_to_lin(hex_srgb(pat["band"])))[None, :] * lum[inb, None]
    a, halo = PATTERNS[pat["kind"]](eu[inb], ev[inb])
    tc = np.asarray(srgb_to_lin(hex_srgb(pat["thread"])))
    stitch = 0.82 + 0.18 * np.sin(eu[inb] * 95.0 + ev[inb] * 30.0) ** 2       # satin-stitch sheen
    c = cur[inb] * (1 - 0.4 * halo)[:, None]
    cur[inb] = c * (1 - a[:, None]) + tc[None, :] * (stitch * (0.9 + 0.1 * lum[inb]))[:, None] * a[:, None]
    base[m] = lin_to_srgb(cur)
    o = orm[m]
    if pat.get("band_roughness") is not None:
        o[inb, 1] = pat["band_roughness"] * (0.85 + 0.3 * (lum[inb] - lum[inb].mean()))
    o[inb, 0] = o[inb, 0] * (1 - 0.3 * halo)
    o[inb, 1] = o[inb, 1] * (1 - a) + pat.get("thread_roughness", 0.36) * a
    o[inb, 2] = o[inb, 2] * (1 - a) + pat.get("thread_metallic", 0.3) * a
    orm[m] = o
    return int(inb.sum())


def lace_alpha(u, v):
    """Black mourning lace: hexagonal mesh of round holes with a floral ring every few cells."""
    x, y = u * 6.0, v * 6.0
    r3 = math.sqrt(3)
    q = (2 / 3 * x)
    rr = (-1 / 3 * x + r3 / 3 * y)
    cq, cr = np.round(q), np.round(rr)
    cx = cq * 1.5
    cy = (cr + cq / 2) * r3
    d = np.hypot(x - cx, y - cy)
    hole = d < 0.55
    flower = (np.mod(cq, 3) == 0) & (np.mod(cr, 3) == 0) & (np.abs(d - 0.38) < 0.08)
    return np.where(hole & ~flower, 0.0, 1.0)


def lace_dense_alpha(u, v):
    """Close mourning lace for bodices: a solid five-petal flower on a brick lattice (one per
    tile) with an open eye and five small openings between the petals, a solid ring round it,
    and an open net of round holes between the flowers (about 70% cover, so it reads as dark
    cloth at fight distance and as lace close up)."""
    row = np.floor(v)
    x = u + 0.5 * (np.mod(row, 2))
    fx, fy = x - np.floor(x) - 0.5, v - row - 0.5
    rho, ang = np.hypot(fx, fy), np.arctan2(fy, fx)
    petal = rho < 0.27 + 0.09 * np.cos(5 * ang)
    eye = rho < 0.05
    gaps = np.zeros_like(rho, bool)
    for k in range(5):
        a = (2 * k + 1) * math.pi / 5
        gaps |= np.hypot(fx - 0.16 * math.cos(a), fy - 0.16 * math.sin(a)) < 0.04
    ring = np.abs(rho - 0.43) < 0.03
    gx, gy = u * 5.0, v * 5.0
    grow = np.floor(gy)
    hx = gx + 0.5 * np.mod(grow, 2)
    net_hole = np.hypot(hx - np.floor(hx) - 0.5, gy - grow - 0.5) < 0.36
    solid = (petal & ~eye & ~gaps) | ring | ~net_hole
    return np.where(solid, 1.0, 0.0)


# ----------------------------------------------------------------------------- output + materials

def write_set(fid, slot, out_dir, base, normal, orm, emissive, alpha):
    files = {}
    p = os.path.join(out_dir, "%s_%s_basecolor.png" % (fid, slot))
    save_png(p, base, alpha=bool(alpha), colorspace="sRGB")
    files["basecolor"] = p
    p = os.path.join(out_dir, "%s_%s_normal.png" % (fid, slot))
    save_png(p, normal, colorspace="Non-Color")
    files["normal"] = p
    p = os.path.join(out_dir, "%s_%s_orm.png" % (fid, slot))
    save_png(p, orm, colorspace="Non-Color")
    files["orm"] = p
    if emissive is not None:
        p = os.path.join(out_dir, "%s_%s_emissive.png" % (fid, slot))
        save_png(p, emissive, colorspace="sRGB")
        files["emissive"] = p
    return files


def build_material(name, files, alpha=False, flip_green=False):
    """Principled material over the slot's PNGs. In the exported .blend/FBX the normal map is
    wired directly (DirectX data, so Blender's own preview shows the green channel inverted);
    render_fighter.py rebuilds the materials with flip_green=True for correct review renders."""
    mat = bpy.data.materials.new(name)
    nt = mat.node_tree
    p = nt.nodes["Principled BSDF"]

    def img(path, cs):
        n = nt.nodes.new("ShaderNodeTexImage")
        n.image = bpy.data.images.load(path, check_existing=True)
        n.image.colorspace_settings.name = cs
        return n

    b = img(files["basecolor"], "sRGB")
    nt.links.new(b.outputs["Color"], p.inputs["Base Color"])
    if alpha:
        nt.links.new(b.outputs["Alpha"], p.inputs["Alpha"])
        mat.surface_render_method = "DITHERED"
    o = img(files["orm"], "Non-Color")
    sep = nt.nodes.new("ShaderNodeSeparateColor")
    nt.links.new(o.outputs["Color"], sep.inputs["Color"])
    nt.links.new(sep.outputs["Green"], p.inputs["Roughness"])
    nt.links.new(sep.outputs["Blue"], p.inputs["Metallic"])
    n = img(files["normal"], "Non-Color")
    nm = nt.nodes.new("ShaderNodeNormalMap")
    if flip_green:
        s2 = nt.nodes.new("ShaderNodeSeparateColor")
        c2 = nt.nodes.new("ShaderNodeCombineColor")
        inv = nt.nodes.new("ShaderNodeMath")
        inv.operation = "SUBTRACT"
        inv.inputs[0].default_value = 1.0
        nt.links.new(n.outputs["Color"], s2.inputs["Color"])
        nt.links.new(s2.outputs["Red"], c2.inputs["Red"])
        nt.links.new(s2.outputs["Green"], inv.inputs[1])
        nt.links.new(inv.outputs[0], c2.inputs["Green"])
        nt.links.new(s2.outputs["Blue"], c2.inputs["Blue"])
        nt.links.new(c2.outputs["Color"], nm.inputs["Color"])
    else:
        nt.links.new(n.outputs["Color"], nm.inputs["Color"])
    nt.links.new(nm.outputs["Normal"], p.inputs["Normal"])
    if "emissive" in files:
        e = img(files["emissive"], "sRGB")
        nt.links.new(e.outputs["Color"], p.inputs["Emission Color"])
        p.inputs["Emission Strength"].default_value = 3.0
    return mat


def texture_and_materials(fid, cfg, built, out_dir):
    objs = built["objects"]
    slots = cfg["slots"]
    groups = {k: [] for k in slots}
    face_slot = "hairface" if "hairface" in slots else "face"
    for key, ob in objs.items():
        surf = built["surface"][ob.name]
        if surf == "body" or cfg.get("surfaces", {}).get(surf, {}).get("slot") == "skin":
            groups["skin"].append(ob)
        elif surf in ("eyes", "brows", "lashes", "hair", "beard", "hair_cap", "braids"):
            groups[face_slot].append(ob)
        else:
            groups[cfg["surfaces"][surf]["slot"]].append(ob)
    report = {"slots": {}, "texture_notes": {}}
    for slot, sp in slots.items():
        size = sp["size"]
        if slot == "skin":
            files, notes = skin_textures(fid, cfg, built, out_dir, size)
            alpha = False
        elif slot == face_slot:
            files, notes = face_atlas(fid, cfg, built, out_dir, slot, size)
            alpha = True
        else:
            files, notes, alpha = tiled_slot(fid, cfg, built, out_dir, slot, groups[slot], size)
        mat = build_material("M_%s_%s" % (fid, slot), files, alpha=alpha)
        for ob in groups[slot]:
            ob.data.materials.clear()
            ob.data.materials.append(mat)
        report["slots"][slot] = {"material": mat.name, "size": size,
                                 "files": {k: os.path.basename(v) for k, v in files.items()},
                                 "objects": [o.name for o in groups[slot]]}
        report["texture_notes"][slot] = notes
        core.log("slot", slot, size, [o.name for o in groups[slot]])
    return report
