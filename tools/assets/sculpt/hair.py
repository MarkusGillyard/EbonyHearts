"""Hair as strand cards: narrow ribbons, each drawn by the hair shader as several strands.

Roots are scattered over the scalp inside the hairline and each strand is combed over the head
(projected onto the head's distance field so it hugs the scalp). Long hair then continues down
a simulated sheet (cage.Grid), following it through the sheet's bones; short hair, brows, lashes
and face-framing locks follow the head alone.
"""
import numpy as np

import mesher


class Strands:
    """Accumulates cards: centrelines with widths, sideways vectors and skin weights."""

    def __init__(self):
        self.v, self.n, self.t, self.uv, self.uv2, self.b, self.w, self.idx = [], [], [], [], [], [], [], []

    def card(self, pts, side, normal, width, bones, weights, seed):
        """`pts` (k, 3) centreline root to tip; `side`, `normal` (k, 3); `width` (k,); bones and
        weights (k, 8) per centreline point."""
        k = len(pts)
        base = len(self.v)
        tan = np.gradient(pts, axis=0)
        tan /= np.maximum(np.linalg.norm(tan, axis=1, keepdims=True), 1e-9)
        for i in range(k):
            for s in (-0.5, 0.5):
                self.v.append(pts[i] + side[i] * width[i] * s)
                self.n.append(normal[i])
                self.t.append(np.append(tan[i], 1.0))
                self.uv.append([s + 0.5, i / (k - 1)])
                self.uv2.append([seed, 0.0])
                self.b.append(bones[i])
                self.w.append(weights[i])
        for i in range(k - 1):
            a = base + 2 * i
            self.idx += [a, a + 2, a + 1, a + 1, a + 2, a + 3]

    def arrays(self, origin=None):
        """Vertex arrays; with `origin`, positions are made relative to it (the anchor joint)."""
        v = np.array(self.v, np.float32)
        if origin is not None:
            v = v - np.asarray(origin, np.float32)
        return {"vertex": v, "normal": np.array(self.n, np.float32),
                "tangent": np.array(self.t, np.float32), "uv": np.array(self.uv, np.float32),
                "uv2": np.array(self.uv2, np.float32), "bones": np.array(self.b, np.int32),
                "weights": np.array(self.w, np.float32), "index": np.array(self.idx, np.int32)}


def hairline(az):
    """Lowest hair height (reference metres above the head joint) at azimuth `az` (0 at the
    front, positive to the fighter's right): forehead, temples, over the ears, nape."""
    return np.interp(np.abs(az), [0.0, 0.5, 0.9, 1.4, 1.75, 2.2, np.pi], [0.168, 0.162, 0.146, 0.132, 0.09, 0.06, 0.045])


class Scalp:
    """The head's surface and hairline, in the head's reference frame (see face.Head)."""

    def __init__(self, head, tree, style):
        self.h = head
        self.tree = tree
        self.style = style
        self.centre = head.P(0, 0.11, 0.01)

    def project(self, p, off):
        """Moves points onto the head's surface, then `off` metres out along its normal."""
        p = mesher.project(self.tree, p, iterations=10)
        n = mesher.normals(self.tree, p)
        return p + n * off[:, None] if np.ndim(off) else p + n * off, n

    def roots(self, count, rng):
        """Random points on the scalp inside the hairline: (points, normals, azimuths)."""
        out = []
        while sum(len(o) for o in out) < count:
            d = rng.normal(size=(count * 3, 3)).astype(np.float32)
            d[:, 1] = np.abs(d[:, 1]) + 0.15
            d /= np.linalg.norm(d, axis=1, keepdims=True)
            p = self.centre + d * self.h.M(0.12)
            p, n = self.project(p, 0.0)
            x, y, z = self.h.to_ref(p)
            az = np.arctan2(x, -z)
            keep = y > hairline(az)
            out.append(np.concatenate([p[keep], n[keep], az[keep, None]], axis=1))
        a = np.concatenate(out)[:count]
        return a[:, :3], a[:, 3:6], a[:, 6]


def _resample(pts, k):
    seg = np.linalg.norm(np.diff(pts, axis=0), axis=1)
    s = np.concatenate([[0], np.cumsum(seg)])
    t = np.linspace(0, s[-1], k)
    return np.stack([np.interp(t, s, pts[:, i]) for i in range(3)], axis=1), s[-1]


def _head_weights(k, head_bone=0):
    b = np.zeros((k, 8), np.int32)
    w = np.zeros((k, 8), np.float32)
    b[:, 0] = head_bone
    w[:, 0] = 1.0
    return b, w


def _over_scalp(sc, a, b, k):
    """Points from a to b around the head rather than through it: directions from the head's
    centre are interpolated on the sphere and distances linearly."""
    c = sc.centre
    da, db = a - c, b - c
    ra, rb = np.linalg.norm(da), np.linalg.norm(db)
    ua, ub = da / ra, db / rb
    ang = np.arccos(np.clip(np.dot(ua, ub), -1.0, 1.0))
    out = []
    for t in np.linspace(0, 1, k):
        if ang < 1e-4:
            d = ua
        else:
            d = (np.sin((1 - t) * ang) * ua + np.sin(t * ang) * ub) / np.sin(ang)
        out.append(c + d * (ra + (rb - ra) * t))
    return np.array(out, np.float32)


def long_hair(head, tree, grid, origin, count, length_jitter, volume, width, rng):
    """Hair combed from the hairline over the crown to the top of `grid`, then down it.
    `grid` is in the anchor joint's frame, which sits at `origin` in rig space; the result is
    built in rig space (call arrays(origin) for the anchor's frame). Bone 0 is the head; grid
    point k is bone k + 1."""
    sc = Scalp(head, tree, "long")
    st = Strands()
    roots, rn, az = sc.roots(count, rng)
    cols = grid.cols
    for i in range(count):
        x, y, z = head.to_ref(roots[i:i + 1])
        # Which column of the sheet this strand ends up in: by its side of the head.
        # Keep strands off the sheet's outer columns, which can swing round the body's sides.
        lateral = np.clip(x[0] / 0.075 + (rng.normal() * 0.12), -1, 1) * 0.78
        u = (cols - 1) * (0.5 + 0.5 * lateral)
        top, tu, tv, tn = grid.frame(u, 0.0)
        top = top + origin
        # Over the scalp: from the root toward the top of the sheet, hugging the head.
        k_s = 7
        layer = head.M(0.002 + rng.uniform(0, 0.007) * volume)
        path = _over_scalp(sc, roots[i], top, k_s)
        off = layer * np.linspace(0.4, 1.0, k_s)
        path, pn = sc.project(path, off)
        path[-1] = top
        # Down the sheet, spread through the hair's thickness.
        depth = rng.uniform(-1, 1) * head.M(0.016) * volume
        v_end = (grid.rows - 1) * (1.0 - rng.uniform(0, length_jitter))
        k_h = max(int(v_end * 2), 4)
        hang, hn, hb, hw, hside = [], [], [], [], []
        for v in np.linspace(0, v_end, k_h):
            p, tu_, tv_, n_ = grid.frame(u, v)
            wave = head.M(0.004) * np.sin(v * 1.7 + i) * (v / (grid.rows - 1))
            hang.append(p + origin + n_ * depth + tu_ * wave)
            hn.append(n_)
            bones, wts = grid.weights(u, v)
            hb.append([b + 1 for b in bones])
            hw.append(wts)
            hside.append(tu_)
        scalp_b, scalp_w = _head_weights(k_s)
        pts = np.concatenate([path[:-1], np.array(hang)], axis=0)
        normal = np.concatenate([pn[:-1], np.array(hn)], axis=0)
        tangent = np.gradient(pts, axis=0)
        side = np.cross(tangent, normal)
        side /= np.maximum(np.linalg.norm(side, axis=1, keepdims=True), 1e-9)
        bones = np.concatenate([scalp_b[:-1], np.array(hb, np.int32)], axis=0)
        weights_ = np.concatenate([scalp_w[:-1], np.array(hw, np.float32)], axis=0)
        n = len(pts)
        taper = np.clip((1.0 - np.linspace(0, 1, n)) * 4.0, 0.25, 1.0)
        wd = head.M(width) * rng.uniform(0.7, 1.3) * taper
        st.card(pts, side, normal, wd, bones, weights_, rng.uniform())
    return st


def short_hair(head, tree, count, length, wave, lift, width, rng):
    """Short hair swept back and up off the forehead, in waves, layered over the scalp."""
    sc = Scalp(head, tree, "short")
    st = Strands()
    roots, rn, az = sc.roots(count, rng)
    for i in range(count):
        r = roots[i]
        x, y, z = head.to_ref(r[None])
        # Comb direction: back over the crown from the front, down and back on the sides.
        fwd = np.array([0, 0, 1.0])
        flow = fwd + np.array([np.sign(x[0]) * 0.25, 0.35 - 0.6 * np.clip((0.14 - y[0]) / 0.06, 0, 1), 0])
        flow /= np.linalg.norm(flow)
        L = head.M(length) * rng.uniform(0.7, 1.25)
        k = 7
        layer = head.M(0.002 + rng.uniform(0, lift))
        pts = []
        for t in np.linspace(0, 1, k):
            ph = t * 2 * np.pi * rng.uniform(0.7, 1.1) + rng.uniform(0, 6.3)
            side_w = np.cross(flow, rn[i])
            pts.append(r + flow * L * t + side_w * head.M(wave) * np.sin(ph))
        pts = np.array(pts, np.float32)
        off = layer + head.M(0.006) * np.sin(np.linspace(0, np.pi, k)) * rng.uniform(0.5, 1.0)
        pts, pn = sc.project(pts, off)
        tangent = np.gradient(pts, axis=0)
        side = np.cross(tangent, pn)
        side /= np.maximum(np.linalg.norm(side, axis=1, keepdims=True), 1e-9)
        b, w = _head_weights(k)
        wd = head.M(width) * rng.uniform(0.7, 1.3) * np.clip((1.0 - np.linspace(0, 1, k)) * 3.0, 0.3, 1.0)
        st.card(pts, side, pn, wd, b, w, rng.uniform())
    return st


def face_strands(head, tree, region, count, length, width, lift, rng, flow_fn):
    """Short strands rooted on the face where `region(x, y, z)` (head reference units) is
    negative: beards, sideburns, moustaches. `flow_fn(x, y, z)` gives each strand's direction in
    the head's frame; strands are laid onto the skin `lift` (reference metres) above it."""
    sc = Scalp(head, tree, "face")
    st = Strands()
    centre = head.P(0, 0.07, -0.01)
    roots = []
    while len(roots) < count:
        d = rng.normal(size=(count * 4, 3)).astype(np.float32)
        d /= np.linalg.norm(d, axis=1, keepdims=True)
        p, n = sc.project(centre + d * head.M(0.1), 0.0)
        x, y, z = head.to_ref(p)
        keep = region(x, y, z) < 0.0
        roots += list(p[keep])
    roots = np.array(roots[:count], np.float32)
    for r in roots:
        x, y, z = head.to_ref(r[None])
        flow = np.asarray(flow_fn(x[0], y[0], z[0]), np.float32)
        flow /= np.linalg.norm(flow)
        L = head.M(length) * rng.uniform(0.6, 1.2)
        k = 5
        pts = np.array([r + flow * L * t for t in np.linspace(0, 1, k)], np.float32)
        off = head.M(lift) * (1.0 + rng.uniform(0, 1.0)) + head.M(0.002) * np.linspace(0, 1, k)
        pts, pn = sc.project(pts, off)
        tangent = np.gradient(pts, axis=0)
        side = np.cross(tangent, pn)
        side /= np.maximum(np.linalg.norm(side, axis=1, keepdims=True), 1e-9)
        b, w = _head_weights(k)
        wd = head.M(width) * rng.uniform(0.7, 1.3) * np.clip((1.0 - np.linspace(0, 1, k)) * 2.5, 0.3, 1.0)
        st.card(pts, side, pn, wd, b, w, rng.uniform())
    return st


def locks(head, tree, st, length, rng, count=40, width=0.008):
    """Locks falling from the temples in front of the ears to the collarbones."""
    sc = Scalp(head, tree, "locks")
    for i in range(count):
        sd = -1 if i % 2 == 0 else 1
        start = head.P(sd * rng.uniform(0.045, 0.06), rng.uniform(0.15, 0.165), rng.uniform(-0.055, -0.03))
        k = 14
        pts = []
        for t in np.linspace(0, 1, k):
            # Out past the ear, then falling clear of the jaw and neck.
            out = 0.012 + 0.034 * np.sin(min(t * 2.2, 1.0) * np.pi * 0.5)
            pts.append(head.P(sd * (0.066 + out), 0.15 - length * t, -0.035 + 0.03 * t + rng.normal() * 0.004))
        pts = np.array(pts, np.float32)
        pts[0] = start
        pts[:4], pn4 = sc.project(pts[:4], head.M(0.003))
        normal = np.tile(np.array([sd, 0.0, -0.4], np.float32) / np.linalg.norm([sd, 0.0, -0.4]), (k, 1))
        normal[:4] = pn4
        tangent = np.gradient(pts, axis=0)
        side = np.cross(tangent, normal)
        side /= np.maximum(np.linalg.norm(side, axis=1, keepdims=True), 1e-9)
        b, w = _head_weights(k)
        wd = head.M(width) * rng.uniform(0.8, 1.3) * np.clip((1.0 - np.linspace(0, 1, k)) * 3.0, 0.3, 1.0)
        st.card(pts, side, normal, wd, b, w, rng.uniform())


def brows_and_lashes(head, tree, rng, brow_amount=1.0):
    """Eyebrows as short strands along the brow ridge, lashes as a strip along each upper lid."""
    st = Strands()
    sc = Scalp(head, tree, "brows")
    for sd in (-1, 1):
        for i in range(int(46 * brow_amount)):
            t = rng.uniform(0, 1)
            x = sd * (0.012 + t * 0.042)
            yb = 0.121 + 0.006 * np.sin(t * np.pi * 0.9) - 0.002 * t
            y = yb + rng.normal() * 0.0018
            root = head.P(x, y, -0.1)
            # Brow hairs point up and outward near the nose, flatten out toward the tail.
            ang = np.radians(70 - 60 * t) if t < 0.3 else np.radians(15 - 10 * t)
            d = np.array([sd * np.cos(ang), np.sin(ang), 0.0])
            k = 4
            pts = np.array([root + d * head.M(0.007) * s for s in np.linspace(0, 1, k)], np.float32)
            pts, pn = sc.project(pts, head.M(np.linspace(0.0004, 0.0012, k)))
            tangent = np.gradient(pts, axis=0)
            side = np.cross(tangent, pn)
            side /= np.maximum(np.linalg.norm(side, axis=1, keepdims=True), 1e-9)
            b, w = _head_weights(k)
            st.card(pts, side, pn, head.M(0.0016) * np.array([1.0, 0.9, 0.7, 0.4]), b, w, rng.uniform())
        # Upper lashes along the lid margin, curling up and out.
        k_along = 14
        xs = np.linspace(-0.0135, 0.0135, k_along)
        root, tip = [], []
        for xr in xs:
            x = sd * (head.eye_sep - 0.0012) + xr
            ys = np.linspace(head.eye_y - 0.004, head.eye_y + 0.012, 60)
            a, _ = head.almond(np.full(60, x), ys, sd)
            j = int(np.argmin(np.abs(a)[30:])) + 30
            yy = ys[j]
            p = head.P(x, yy, head.eye_z - 0.0128)
            root.append(p)
            tip.append(p + head.P(0, 0.0045, -0.0035) - head.P(0, 0, 0) + np.array([sd * xr * 0.15, 0, 0], np.float32) * head.hs)
        root, tip = np.array(root, np.float32), np.array(tip, np.float32)
        for j in range(k_along - 1):
            pts = np.array([(root[j] + root[j + 1]) * 0.5, (tip[j] + tip[j + 1]) * 0.5], np.float32)
            side = np.tile((root[j + 1] - root[j]) / np.linalg.norm(root[j + 1] - root[j]), (2, 1))
            normal = np.tile(np.array([0, 0.3, -1.0], np.float32) / np.linalg.norm([0, 0.3, -1.0]), (2, 1))
            b, w = _head_weights(2)
            st.card(pts, side, normal, np.full(2, float(np.linalg.norm(root[j + 1] - root[j])) * 1.15), b, w, rng.uniform())
    return st


def hairline_mask(head, p):
    """1 on the scalp where hair grows, for tinting the skin beneath the strands."""
    x, y, z = head.to_ref(p)
    az = np.arctan2(x, -z)
    return np.clip((y - hairline(az)) / 0.006 + 0.5, 0, 1) * (np.linalg.norm(np.stack([x, y - 0.11, z], 1), axis=1) > 0.05)

