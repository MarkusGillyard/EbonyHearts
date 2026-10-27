"""Builds the game's imported content from the versioned sources. Run in the editor commandlet:

    source Tools/ue.sh && SLOP_IMPORT_ONLY=audio,materials editor_py "$PROJ_DIR/Tools/import_assets.py"

SLOP_IMPORT_ONLY (comma list of steps: audio, materials, bodies, fighters, surfaces, props,
stages) limits the run; without it every step runs.

Sources: SourceArt/Audio (first-party), SourceArt/ThirdParty (CC0, see ledger.json),
SourceArt/Fighters/<id>/<id>.fbx (built by Tools/blender/build_fighter.py). Everything lands
under /Game/Slop and is overwritten in place, so references stay valid.
"""
import json
import os
import sys

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "SourceArt")
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log_warning("[Import] " + msg)


def args():
    only = os.environ.get("SLOP_IMPORT_ONLY", "").strip()
    return set(only.split(",")) if only else None


def import_files(files, dest, replace=True):
    """Imports files (absolute paths) into the content folder `dest`; returns object paths."""
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", f)
        t.set_editor_property("destination_path", dest)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", replace)
        t.set_editor_property("save", True)
        tasks.append(t)
    TOOLS.import_asset_tasks(tasks)
    out = []
    for t in tasks:
        out.extend(t.get_editor_property("imported_object_paths"))
    return out


# ------------------------------------------------------------------------------ audio
def audio():
    orig = os.path.join(SRC, "Audio", "Original")
    files = sorted(os.path.join(orig, f) for f in os.listdir(orig) if f.endswith(".wav"))
    got = import_files(files, "/Game/Slop/Audio/Original")
    log(f"original sounds: {len(got)} of {len(files)}")
    kenney = os.path.join(SRC, "ThirdParty", "Kenney")
    for pack, sub in [("impact-sounds", "Impact"), ("interface-sounds", "Interface"), ("rpg-audio", "RPG")]:
        root = os.path.join(kenney, pack)
        oggs = []
        for d, _, fs in os.walk(root):
            oggs += [os.path.join(d, f) for f in fs if f.endswith(".ogg")]
        got = import_files(sorted(oggs), "/Game/Slop/Audio/Kenney/" + sub)
        log(f"kenney {pack}: {len(got)} of {len(oggs)}")
    # Music loops forever; effects never.
    for p in EAL.list_assets("/Game/Slop/Audio", recursive=True, include_folder=False):
        a = EAL.load_asset(p)
        if isinstance(a, unreal.SoundWave):
            a.set_editor_property("looping", os.path.basename(p).split(".")[0].startswith("music_"))
            EAL.save_loaded_asset(a)


# ------------------------------------------------------------------------------ materials
def fresh_material(path):
    """The material at `path`, emptied for rebuilding; kept in place so instances stay parented."""
    if EAL.does_asset_exist(path):
        m = EAL.load_asset(path)
        MEL.delete_all_material_expressions(m)
        return m
    d, n = path.rsplit("/", 1)
    return TOOLS.create_asset(n, d, unreal.Material, unreal.MaterialFactoryNew())


def expr(m, cls, x, y):
    return MEL.create_material_expression(m, cls, x, y)


def scalar(m, name, default, x, y):
    e = expr(m, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", default)
    return e


def vector(m, name, default, x, y):
    e = expr(m, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(*default))
    return e


def texparam(m, name, default_path, x, y, sampler=None):
    e = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("texture", EAL.load_asset(default_path))
    if sampler is not None:
        e.set_editor_property("sampler_type", sampler)
    return e


def mul(m, a, b, x, y, ao="", bo=""):
    e = expr(m, unreal.MaterialExpressionMultiply, x, y)
    MEL.connect_material_expressions(a, ao, e, "A")
    MEL.connect_material_expressions(b, bo, e, "B")
    return e


def add(m, a, b, x, y, ao="", bo=""):
    e = expr(m, unreal.MaterialExpressionAdd, x, y)
    MEL.connect_material_expressions(a, ao, e, "A")
    MEL.connect_material_expressions(b, bo, e, "B")
    return e


def lerp(m, a, b, alpha, x, y, ao="", bo="", alo=""):
    e = expr(m, unreal.MaterialExpressionLinearInterpolate, x, y)
    MEL.connect_material_expressions(a, ao, e, "A")
    MEL.connect_material_expressions(b, bo, e, "B")
    MEL.connect_material_expressions(alpha, alo, e, "Alpha")
    return e


WHITE = "/Engine/EngineResources/WhiteSquareTexture"
WHITE_LINEAR = "/Game/Slop/Materials/T_WhiteLinear"


def white_linear():
    """A small white texture stored linear (masks), the default for ORM samplers."""
    import struct, tempfile, zlib
    raw = b"".join(b"\x00" + b"\xff\xff\xff" * 4 for _ in range(4))
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 4, 4, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
    d = tempfile.mkdtemp()
    path = os.path.join(d, "T_WhiteLinear.png")
    with open(path, "wb") as f:
        f.write(png)
    t = import_as(path, "/Game/Slop/Materials", "T_WhiteLinear")
    t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    t.set_editor_property("srgb", False)
    EAL.save_loaded_asset(t)
FLAT_NORMAL = "/Engine/EngineMaterials/DefaultNormal"
GREY = "/Engine/EngineResources/DefaultTexture"


def character_material():
    """M_Character: base colour x tint, normal, ORM; the fighter's accent rim; a hit flash; a tint
    toward a colour (the Reflection, ash, salt); a dissolve with a glowing edge (finishers)."""
    m = fresh_material("/Game/Slop/Materials/M_Character")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("used_with_skeletal_mesh", True)
    base = texparam(m, "BaseColorTex", WHITE, -1400, -400)
    tint = vector(m, "Tint", (1, 1, 1, 1), -1400, -150)
    col = mul(m, base, tint, -1100, -300, "RGB", "")
    towards = vector(m, "TintColor", (0.05, 0.08, 0.05, 1), -1100, -100)
    amount = scalar(m, "TintAmount", 0.0, -1100, 0)
    col2 = lerp(m, col, towards, amount, -850, -200)
    MEL.connect_material_property(col2, "", unreal.MaterialProperty.MP_BASE_COLOR)
    normal = texparam(m, "NormalTex", FLAT_NORMAL, -1400, 150, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    MEL.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    orm = texparam(m, "ORMTex", WHITE_LINEAR, -1400, 450, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    rough_k = scalar(m, "Roughness", 0.6, -1100, 400)
    metal_k = scalar(m, "Metallic", 0.0, -1100, 520)
    rough = mul(m, orm, rough_k, -850, 400, "G", "")
    gloss = scalar(m, "TintGloss", 0.0, -1100, 300)
    rough2 = lerp(m, rough, gloss, amount, -650, 400)
    one = expr(m, unreal.MaterialExpressionConstant, -850, 300)
    one.set_editor_property("r", 0.08)
    MEL.connect_material_expressions(one, "", rough2, "B")
    MEL.connect_material_property(rough2, "", unreal.MaterialProperty.MP_ROUGHNESS)
    metal = mul(m, orm, metal_k, -850, 520, "B", "")
    MEL.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    MEL.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    # Emissive: accent rim (fresnel) + flash + dissolve edge + self glow (eyes, embers).
    fres = expr(m, unreal.MaterialExpressionFresnel, -1100, 700)
    fres.set_editor_property("exponent", 3.0)
    accent = vector(m, "AccentColor", (1, 1, 1, 1), -1100, 820)
    accent_k = scalar(m, "AccentStrength", 0.24, -1100, 940)
    rim = mul(m, mul(m, fres, accent, -850, 760), accent_k, -650, 800)
    flash = scalar(m, "Flash", 0.0, -1100, 1060)
    flash_col = vector(m, "FlashColor", (1, 0.95, 0.85, 1), -1100, 1160)
    fl = mul(m, flash_col, flash, -850, 1100)
    glow = scalar(m, "Glow", 0.0, -1100, 1280)
    glow_e = mul(m, col, glow, -850, 1260)
    em = add(m, add(m, rim, fl, -450, 900), glow_e, -250, 1000)
    # Dissolve: world-space noise against a threshold; the band just above it glows.
    noise = expr(m, unreal.MaterialExpressionNoise, -1400, 1500)
    noise.set_editor_property("scale", 0.06)
    noise.set_editor_property("quality", 1)
    noise.set_editor_property("output_min", 0.0)
    noise.set_editor_property("output_max", 1.0)
    dissolve = scalar(m, "Dissolve", 0.0, -1400, 1700)
    diff = expr(m, unreal.MaterialExpressionSubtract, -1100, 1550)
    MEL.connect_material_expressions(noise, "", diff, "A")
    MEL.connect_material_expressions(dissolve, "", diff, "B")
    MEL.connect_material_property(diff, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    edge = expr(m, unreal.MaterialExpressionOneMinus, -850, 1650)
    sat = expr(m, unreal.MaterialExpressionSaturate, -1000, 1650)
    band = mul(m, diff, scalar(m, "EdgeWidth", 14.0, -1100, 1800), -1050, 1700)
    MEL.connect_material_expressions(band, "", sat, "")
    MEL.connect_material_expressions(sat, "", edge, "")
    on = expr(m, unreal.MaterialExpressionCeil, -850, 1780)
    MEL.connect_material_expressions(dissolve, "", on, "")
    edge_col = vector(m, "DissolveColor", (1, 0.45, 0.1, 1), -850, 1880)
    e1 = mul(m, mul(m, edge, on, -650, 1700), edge_col, -450, 1750)
    e2 = mul(m, e1, scalar(m, "EdgeGlow", 8.0, -650, 1880), -300, 1800)
    em2 = add(m, em, e2, -100, 1200)
    MEL.connect_material_property(em2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log("M_Character built")


def surface_material():
    """M_Surface: tiled stage surfaces (colour, normal, ORM) with a tint, world-aligned option off."""
    m = fresh_material("/Game/Slop/Materials/M_Surface")
    tc = expr(m, unreal.MaterialExpressionTextureCoordinate, -1600, 0)
    tiling = scalar(m, "Tiling", 1.0, -1600, 150)
    uv = mul(m, tc, tiling, -1400, 50)
    def sample(name, default, y, sampler=None):
        e = texparam(m, name, default, -1150, y, sampler)
        MEL.connect_material_expressions(uv, "", e, "UVs")
        return e
    base = sample("BaseColorTex", WHITE, -300)
    tint = vector(m, "Tint", (1, 1, 1, 1), -1150, -60)
    col = mul(m, base, tint, -850, -200, "RGB", "")
    MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    nrm = sample("NormalTex", FLAT_NORMAL, 100, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    flat = expr(m, unreal.MaterialExpressionConstant3Vector, -850, 250)
    flat.set_editor_property("constant", unreal.LinearColor(0, 0, 1, 1))
    nk = scalar(m, "NormalStrength", 1.0, -850, 330)
    nrm2 = lerp(m, flat, nrm, nk, -600, 150, "", "RGB", "")
    MEL.connect_material_property(nrm2, "", unreal.MaterialProperty.MP_NORMAL)
    orm = sample("ORMTex", WHITE_LINEAR, 450, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    rk = scalar(m, "Roughness", 1.0, -850, 450)
    MEL.connect_material_property(mul(m, orm, rk, -600, 450, "G", ""), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mk = scalar(m, "Metallic", 0.0, -850, 570)
    MEL.connect_material_property(mul(m, orm, mk, -600, 570, "B", ""), "", unreal.MaterialProperty.MP_METALLIC)
    MEL.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    em_col = vector(m, "EmissiveColor", (0, 0, 0, 1), -850, 700)
    em_k = scalar(m, "Emissive", 0.0, -850, 820)
    MEL.connect_material_property(mul(m, mul(m, em_col, em_k, -600, 750), base, -400, 750, "", "RGB"), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log("M_Surface built")


def glow_material(path, blend, two_sided=True):
    """Unlit, coloured, optionally additive: effects, lantern glass, slop, lightning."""
    m = fresh_material(path)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", blend)
    m.set_editor_property("two_sided", two_sided)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    col = vector(m, "Color", (1, 1, 1, 1), -800, 0)
    k = scalar(m, "Intensity", 1.0, -800, 200)
    vc = expr(m, unreal.MaterialExpressionVertexColor, -800, 350)
    e = mul(m, mul(m, col, k, -500, 100), vc, -300, 200, "", "RGB")
    MEL.connect_material_property(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if blend != unreal.BlendMode.BLEND_OPAQUE:
        op = scalar(m, "Opacity", 1.0, -800, 500)
        MEL.connect_material_property(mul(m, op, vc, -500, 500, "", "A"), "", unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)


def billboard_material(path, ring):
    """Additive camera-facing quads: a soft disc (flashes, puffs) or a thin ring (impact rings),
    shaped from the quad's UVs; colour x intensity x vertex alpha."""
    m = fresh_material(path)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    tc = expr(m, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
    c = expr(m, unreal.MaterialExpressionConstant2Vector, -1400, 150)
    c.set_editor_property("r", 0.5)
    c.set_editor_property("g", 0.5)
    dist = expr(m, unreal.MaterialExpressionDistance, -1150, 50)
    MEL.connect_material_expressions(tc, "", dist, "A")
    MEL.connect_material_expressions(c, "", dist, "B")
    two = expr(m, unreal.MaterialExpressionConstant, -1150, 200)
    two.set_editor_property("r", 2.0)
    r = mul(m, dist, two, -950, 100)
    if ring:
        # |r - 0.8| narrow band.
        k = expr(m, unreal.MaterialExpressionConstant, -950, 250)
        k.set_editor_property("r", 0.8)
        sub = expr(m, unreal.MaterialExpressionSubtract, -800, 150)
        MEL.connect_material_expressions(r, "", sub, "A")
        MEL.connect_material_expressions(k, "", sub, "B")
        ab = expr(m, unreal.MaterialExpressionAbs, -650, 150)
        MEL.connect_material_expressions(sub, "", ab, "")
        w = scalar(m, "Width", 8.0, -650, 300)
        band = mul(m, ab, w, -500, 200)
        one_m = expr(m, unreal.MaterialExpressionOneMinus, -350, 200)
        MEL.connect_material_expressions(band, "", one_m, "")
        shape_in = one_m
    else:
        one_m = expr(m, unreal.MaterialExpressionOneMinus, -800, 150)
        MEL.connect_material_expressions(r, "", one_m, "")
        shape_in = one_m
    sat = expr(m, unreal.MaterialExpressionSaturate, -200, 200)
    MEL.connect_material_expressions(shape_in, "", sat, "")
    pw = expr(m, unreal.MaterialExpressionPower, -50, 200)
    MEL.connect_material_expressions(sat, "", pw, "Base")
    ex = scalar(m, "Sharpness", 2.0, -200, 350)
    MEL.connect_material_expressions(ex, "", pw, "Exp")
    col = vector(m, "Color", (1, 1, 1, 1), -200, -150)
    k2 = scalar(m, "Intensity", 2.0, -200, -50)
    # Per-instance fade: custom data 0 (1 when unused).
    fade = expr(m, unreal.MaterialExpressionPerInstanceCustomData, -200, 500)
    fade.set_editor_property("data_index", 0)
    fade.set_editor_property("const_default_value", 1.0)
    e = mul(m, mul(m, mul(m, col, k2, 50, -100), pw, 250, 0), fade, 450, 100)
    MEL.connect_material_property(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)


def materials():
    white_linear()
    billboard_material("/Game/Slop/Materials/M_SoftDisc", False)
    billboard_material("/Game/Slop/Materials/M_Ring", True)
    character_material()
    surface_material()
    glow_material("/Game/Slop/Materials/M_GlowAdd", unreal.BlendMode.BLEND_ADDITIVE)
    glow_material("/Game/Slop/Materials/M_GlowTranslucent", unreal.BlendMode.BLEND_TRANSLUCENT)
    glow_material("/Game/Slop/Materials/M_GlowOpaque", unreal.BlendMode.BLEND_OPAQUE, False)
    log("materials done")


# ------------------------------------------------------------------------------ fighters
def texture(path_png, dest, srgb=True, normal=False):
    got = import_files([path_png], dest)
    if not got:
        return None
    t = EAL.load_asset(got[0])
    if normal:
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        t.set_editor_property("srgb", False)
    elif not srgb:
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        t.set_editor_property("srgb", False)
    EAL.save_loaded_asset(t)
    return t


def fighter(fid, fbx):
    dest = f"/Game/Slop/Fighters/{fid}"
    folder = os.path.dirname(fbx)
    got = import_files([fbx], dest)
    meshes = [EAL.load_asset(p) for p in got if isinstance(EAL.load_asset(p), unreal.SkeletalMesh)]
    if not meshes:
        log(f"{fid}: no skeletal mesh imported from {fbx}")
        return
    mesh = meshes[0]
    # One material instance per slot, from the textures beside the FBX named after the slot.
    base = EAL.load_asset("/Game/Slop/Materials/M_Character")
    mats = mesh.get_editor_property("materials")
    for i, sm in enumerate(mats):
        slot = str(sm.get_editor_property("material_slot_name"))
        if not any(c.lower().endswith(".png") and slot.lower() in c.lower() for c in os.listdir(folder)):
            # No textures of ours for this slot (development bodies): keep what the FBX brought.
            continue
        mi_path = f"{dest}/MI_{fid}_{slot}"
        if EAL.does_asset_exist(mi_path):
            mi = EAL.load_asset(mi_path)
        else:
            mi = TOOLS.create_asset(f"MI_{fid}_{slot}", dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(mi, base)
        for suffix, param, kind in [("basecolor", "BaseColorTex", "srgb"), ("normal", "NormalTex", "normal"), ("orm", "ORMTex", "linear")]:
            png = None
            for cand in os.listdir(folder):
                low = cand.lower()
                if low.endswith(f"_{suffix}.png") and slot.lower() in low:
                    png = os.path.join(folder, cand)
            if png:
                t = texture(png, dest + "/Textures", srgb=(kind == "srgb"), normal=(kind == "normal"))
                if t:
                    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, param, t)
        EAL.save_loaded_asset(mi)
        sm.set_editor_property("material_interface", mi)
        mats[i] = sm
    mesh.set_editor_property("materials", mats)
    EAL.save_loaded_asset(mesh)
    log(f"{fid}: {mesh.get_path_name()} with {len(mats)} material slots")


def fighters():
    root = os.path.join(SRC, "Fighters")
    if not os.path.isdir(root):
        log("no SourceArt/Fighters yet")
        return
    for fid in sorted(os.listdir(root)):
        fbx = os.path.join(root, fid, fid + ".fbx")
        if os.path.exists(fbx):
            fighter(fid, fbx)


def bodies():
    """Development bodies from SourceArt/Bodies (before costumes exist)."""
    root = os.path.join(SRC, "Bodies")
    for f in sorted(os.listdir(root)):
        if f.endswith(".fbx"):
            fighter("body_" + f[:-4], os.path.join(root, f))


# ------------------------------------------------------------------------------ stages
def import_as(src, dest_dir, name):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", src)
    t.set_editor_property("destination_path", dest_dir)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    TOOLS.import_asset_tasks([t])
    paths = t.get_editor_property("imported_object_paths")
    return EAL.load_asset(paths[0]) if paths else None


def surfaces():
    """SourceArt/Surfaces (prepared by Tools/prep_surfaces.py) -> /Game/Slop/Surfaces/<Set>/T_<Set>_*."""
    root = os.path.join(SRC, "Surfaces")
    n = 0
    for set_id in sorted(os.listdir(root)):
        d = os.path.join(root, set_id)
        dest = f"/Game/Slop/Surfaces/{set_id}"
        for suffix, name, kind in [("basecolor", "BaseColor", "srgb"), ("normal", "Normal", "normal"), ("orm", "ORM", "linear")]:
            png = os.path.join(d, f"{set_id}_{suffix}.png")
            if not os.path.exists(png):
                continue
            tex = import_as(png, dest, f"T_{set_id}_{name}")
            if not tex:
                log(f"{set_id}: {suffix} failed")
                continue
            if kind == "normal":
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                tex.set_editor_property("srgb", False)
            elif kind == "linear":
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
                tex.set_editor_property("srgb", False)
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
            EAL.save_loaded_asset(tex)
            n += 1
    log(f"surfaces: {n} textures")


def props():
    """Poly Haven models (glTF) -> /Game/Slop/Props/<name>; skies (.hdr) -> /Game/Slop/Skies."""
    models = os.path.join(SRC, "ThirdParty", "PolyHaven", "models")
    for name in sorted(os.listdir(models)):
        d = os.path.join(models, name)
        gltf = None
        for dd, _, fs in os.walk(d):
            for f in fs:
                if f.endswith(".gltf") or f.endswith(".glb"):
                    gltf = os.path.join(dd, f)
        if gltf:
            got = import_files([gltf], f"/Game/Slop/Props/{name}")
            log(f"prop {name}: {len(got)} assets")
    skies = os.path.join(SRC, "ThirdParty", "PolyHaven", "hdris")
    for name in sorted(os.listdir(skies)):
        for f in os.listdir(os.path.join(skies, name)):
            if f.endswith(".hdr"):
                tex = import_as(os.path.join(skies, name, f), "/Game/Slop/Skies", f"HDR_{name}")
                log(f"sky {name}: {'ok' if tex else 'failed'}")


def main():
    only = args()
    steps = [("audio", audio), ("materials", materials), ("bodies", bodies), ("fighters", fighters),
             ("surfaces", surfaces), ("props", props)]
    for name, fn in steps:
        if only is None or name in only:
            log("step " + name)
            fn()
    log("all done")


main()
