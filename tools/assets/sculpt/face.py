"""The head: a smooth skull and face built from large blended volumes, with the facial
features (brow, nose, cheekbones, lips, chin, lid creases) added as a displacement of that
surface. Eyelids drape over the eyeballs and an almond opening shows the eye. Coordinates are
the head's frame (origin at the head joint, y up, -z forward) in metres for a 1.8 m adult,
scaled per fighter.
"""
import numpy as np

from sdf import Leaf, Node, Subtract, Union, curve_tube, ellipsoid, sphere


def g2(x, y, cx, cy, sx, sy):
    return np.exp(-(((x - cx) / sx) ** 2 + ((y - cy) / sy) ** 2))


def band(v, lo, hi, soft):
    """1 inside [lo, hi], fading to 0 over `soft`."""
    return np.clip((v - lo) / soft + 0.5, 0, 1) * np.clip((hi - v) / soft + 0.5, 0, 1)


class Head:
    def __init__(self, body):
        self.b = body
        f = body.female
        fc = body.face
        self.f = f
        self.hs = body.s * body.head_scale
        self.origin = body.rig.pos("head")
        self.jaw = float(fc.get("jaw", 0.9 if f else 1.06))
        self.brow = float(fc.get("brow", 0.3 if f else 1.0))
        self.nose = float(fc.get("nose", 0.88 if f else 1.0))
        self.lips = float(fc.get("lips", 1.2 if f else 0.95))
        self.cheek = float(fc.get("cheek", 1.0))
        self.chin = float(fc.get("chin", 0.85 if f else 1.05))
        self.eye_open = float(fc.get("eye_open", 1.0))
        self.tilt = float(fc.get("tilt", 5.0 if f else 2.5))
        self.eye_sep = float(fc.get("eye_sep", 0.0315))
        self.eye_y = float(fc.get("eye_y", 0.104))
        self.eye_z = float(fc.get("eye_z", -0.071))
        self.eye_r = 0.0118
        self.smile = float(fc.get("smile", 0.0))

    # --- the face's features, in reference units ----------------------------------------------

    def almond(self, x, y, sd):
        """Signed 2D distance to the eye opening (negative inside) and the height above its
        centre line."""
        cx = sd * (self.eye_sep - 0.0012)
        cy = self.eye_y + 0.0006
        w = 0.0142
        hgt = 0.0058 * self.eye_open
        t = np.radians(self.tilt) * sd
        xr = (x - cx) * np.cos(t) + (y - cy) * np.sin(t)
        yr = -(x - cx) * np.sin(t) + (y - cy) * np.cos(t)
        # The upper lid peaks toward the nose and the lower lid dips toward the outer corner.
        q = np.clip(1 - (xr / w) ** 2, 0.0, 1.0)
        xr_up = xr + sd * 0.0025 * q
        upper_r = (w * w + hgt * hgt) / (2 * hgt)
        lo_h = hgt * 0.6
        lower_r = (w * w + lo_h * lo_h) / (2 * lo_h)
        d_up = np.sqrt(xr_up ** 2 + (yr - (hgt - upper_r)) ** 2) - upper_r
        xr_lo = xr - sd * 0.002 * q
        d_lo = np.sqrt(xr_lo ** 2 + (yr + (lo_h - lower_r)) ** 2) - lower_r
        return np.maximum(d_up, d_lo), yr

    def offset(self, x, y):
        """Outward displacement of the face surface at (x, y), in reference metres."""
        f = self.f
        n = self.nose
        o = np.zeros_like(x)
        for sd in (-1, 1):
            o += 0.005 * self.cheek * g2(x, y, sd * 0.05, 0.086, 0.017, 0.012)           # cheekbone
            o -= (0.0015 if f else 0.003) * g2(x, y, sd * 0.046, 0.058, 0.013, 0.014)     # cheek hollow
            o += 0.0085 * g2(x, y, sd * 0.0128 * n, 0.0672, 0.0058 * n, 0.0058)           # nose wing
            o -= 0.0012 * g2(x, y, sd * 0.0192 * n, 0.0685, 0.0018, 0.0055)               # crease round the wing
            o -= 0.0011 * g2(x, y, sd * 0.0068, 0.0618, 0.0022, 0.0012)                   # nostril
            o -= 0.0022 * g2(x, y, sd * 0.0245, 0.04 + 0.002 * self.smile, 0.0035, 0.004)  # mouth corner
            o += 0.001 * g2(x, y, sd * 0.022, 0.052, 0.004, 0.012)                        # nasolabial fold
            a, yr = self.almond(x, y, sd)
            o -= 0.0015 * np.exp(-((a - 0.0068) / 0.0022) ** 2) * np.clip(yr / 0.004 + 0.5, 0.0, 1.0)  # upper lid crease
        o += 0.0045 * self.brow * np.exp(-((y - 0.121) / 0.0085) ** 2) * band(np.abs(x), -1, 0.05, 0.012)
        o += 0.0015 * g2(x, y, 0, 0.116, 0.01, 0.006)                                       # glabella
        # Nose: the ridge rises from the root between the eyes to the tip, then turns under.
        t = np.clip((0.114 - y) / (0.114 - 0.074), 0.0, 1.0)
        height = (0.002 + 0.019 * t ** 1.25) * n
        below = np.clip((0.074 - y) / 0.013, 0.0, 1.0)
        height = height * (1.0 - below ** 1.6 * 0.62) * np.clip((y - 0.0575) / 0.003, 0.0, 1.0)
        width = (0.0078 + 0.0035 * t ** 2 + 0.002 * below) * n
        o += height * np.exp(-(x / width) ** 2) * band(y, 0.057, 0.13, 0.004)
        o += 0.0062 * n * g2(x, y, 0, 0.0735, 0.0098 * n, 0.0078)                          # rounded tip
        # Mouth: the lips and the line between them.
        o += 0.005 * g2(x, y, 0, 0.042, 0.028, 0.024)
        # Upper lip with a cupid's bow, fuller lower lip, a crisp border to each.
        bow = 0.0016 * np.cos(np.clip(x / 0.0072, -np.pi, np.pi)) * (np.abs(x) < 0.022)
        up_c = 0.0458 + bow * 0.5
        taper_u = np.clip(1.0 - 0.5 * (x / 0.0225) ** 2, 0.2, 1.0)
        o += 0.0038 * self.lips * np.exp(-((y - up_c) / (0.0046 * taper_u)) ** 2 - (x / 0.0225) ** 4)
        taper_l = np.clip(1.0 - 0.55 * (x / 0.0205) ** 2, 0.2, 1.0)
        o += 0.0045 * self.lips * np.exp(-((y - 0.0337) / (0.0052 * taper_l)) ** 2 - (x / 0.0205) ** 4)
        line_y = 0.0398 + 6.0 * x * x * (self.smile * 0.6 - 0.1)
        # A crease rather than a slit: a deep narrow cut leaves steep walls that catch the light.
        o -= 0.0016 * np.exp(-((y - line_y) / 0.0011) ** 2) * band(np.abs(x), -1, 0.0225, 0.003)
        o -= 0.0011 * np.exp(-(x / 0.0034) ** 2) * band(y, 0.05, 0.059, 0.003)              # philtrum
        o -= 0.003 * g2(x, y, 0, 0.0215, 0.018, 0.0042)                                     # under the lip
        o += 0.004 * self.chin * g2(x, y, 0, 0.0065, 0.016, 0.012)                          # chin
        return o

    # --- distance fields ---------------------------------------------------------------------

    def to_ref(self, p):
        q = (p - self.origin) / self.hs
        return q[:, 0], q[:, 1], q[:, 2]

    def P(self, x, y, z):
        return self.origin + np.array([x, y, z], np.float32) * self.hs

    def M(self, v):
        return v * self.hs

    def node(self, mat="skin"):
        P, M = self.P, self.M
        j = self.jaw
        hb = ["head"]
        base = Union([
            ellipsoid(P(0, 0.122, 0.018), [M(0.075), M(0.094), M(0.092)], mat, hb),            # cranium
            ellipsoid(P(0, 0.142, -0.02), [M(0.066), M(0.062), M(0.074)], mat, hb),            # forehead
            ellipsoid(P(0, 0.07, -0.045), [M(0.062), M(0.058), M(0.05)], mat, hb),             # mid-face
            ellipsoid(P(0, 0.03, -0.047), [M(0.048 * j), M(0.03), M(0.042)], mat, hb),        # jaw
            ellipsoid(P(0, 0.008, -0.077), [M(0.023 * self.chin ** 0.5), M(0.019), M(0.017)], mat, hb),  # chin
        ] + [ellipsoid(P(sd * 0.06, 0.122, -0.034), [M(0.016), M(0.034), M(0.034)], mat, hb) for sd in (-1, 1)]
          + [ellipsoid(P(sd * 0.048 * j, 0.04, -0.014), [M(0.013), M(0.024), M(0.022)], mat, hb) for sd in (-1, 1)],
            M(0.02))
        head = _Relief(base, self)
        eyes, er = self.eyes()
        lids = [sphere(c, er + M(0.0024), mat, hb, tag="lid") for c in eyes]
        head = Union([head] + lids, M(0.006))
        cuts = [self.opening(c, er, sd) for c, sd in zip(eyes, (-1, 1))]
        head = Subtract(head, Union(cuts, 0.0), M(0.0012))
        ears = [self.ear(sd, mat) for sd in (-1, 1)]
        return Union([head] + ears, M(0.004))

    def opening(self, c, er, sd):
        """The almond opening through the lids, cut only down to just inside the eyeball so the
        lid margins wrap onto it."""
        hs = self.hs

        def fn(p):
            x, y, z = self.to_ref(p)
            a, _ = self.almond(x, y, sd)
            inside_ball = er - 0.0005 * hs - np.linalg.norm(p - c, axis=1)
            behind = p[:, 2] - c[2]
            return np.maximum(np.maximum(a * hs, inside_ball), behind).astype(np.float32)

        lo = c - np.array([0.02, 0.012, 0.03], np.float32) * hs
        hi = c + np.array([0.02, 0.012, 0.0], np.float32) * hs
        return Leaf(fn, lo, hi, "", [], "eye_opening")

    def ear(self, sd, mat):
        P, M = self.P, self.M
        c = np.array([sd * 0.071, 0.092, 0.01])
        up = np.array([0.0, 1.0, 0.25])
        up /= np.linalg.norm(up)
        out = np.array([sd, 0.0, 0.3])
        out = out - up * np.dot(out, up)
        out /= np.linalg.norm(out)
        fwd = np.cross(up, out) * sd

        def E(u, v, w):
            return P(*(c + up * v + fwd * u + out * w))

        rim = [E(0.012, -0.004, 0.006), E(0.014, 0.012, 0.007), E(0.008, 0.026, 0.008), E(-0.004, 0.029, 0.009),
               E(-0.013, 0.02, 0.0095), E(-0.016, 0.004, 0.009), E(-0.012, -0.014, 0.008), E(-0.004, -0.026, 0.006)]
        radii = [M(0.0026), M(0.0034), M(0.0036), M(0.0036), M(0.0036), M(0.0034), M(0.0032), M(0.0045)]
        rot = np.stack([fwd, up, out], axis=1)
        plate = ellipsoid(E(-0.002, 0.002, 0.003), [M(0.015), M(0.028), M(0.005)], mat, ["head"], rot=rot)
        lobe = ellipsoid(E(-0.004, -0.022, 0.005), [M(0.007), M(0.008), M(0.004)], mat, ["head"], rot=rot)
        tragus = ellipsoid(E(0.012, -0.002, 0.004), [M(0.003), M(0.005), M(0.003)], mat, ["head"], rot=rot)
        rim, radii = _smooth_curve(rim, radii, 4)
        helix = curve_tube(rim, radii, mat, ["head"], k=M(0.002))
        shape = Union([plate, lobe, helix, tragus], M(0.003))
        bowl = ellipsoid(E(0.0, 0.002, 0.009), [M(0.009), M(0.015), M(0.0045)], mat, ["head"], rot=rot, tag="concha")
        return Subtract(shape, bowl, M(0.0015))

    def eyes(self):
        return [self.P(sd * self.eye_sep, self.eye_y, self.eye_z) for sd in (-1, 1)], self.M(self.eye_r)

    def lip_mask(self, p):
        """1 on the red of the lips, 0 elsewhere, for colouring vertices."""
        x, y, z = self.to_ref(p)
        lipy = np.where(y > 0.0398, (y - 0.0468) / 0.0062, (y - 0.0335) / 0.0072)
        m = np.exp(-lipy ** 2 * 1.5 - (x / 0.022) ** 6) * (z < -0.08)
        return np.clip(m * 1.4 - 0.2, 0, 1)


def _smooth_curve(points, radii, sub):
    """Catmull-Rom subdivision of a polyline and its radii."""
    pts = [np.asarray(p, np.float32) for p in points]
    out_p, out_r = [], []
    for i in range(len(pts) - 1):
        p0 = pts[max(i - 1, 0)]
        p1, p2 = pts[i], pts[i + 1]
        p3 = pts[min(i + 2, len(pts) - 1)]
        for k in range(sub):
            t = k / sub
            t2, t3 = t * t, t * t * t
            out_p.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3))
            out_r.append(radii[i] + (radii[i + 1] - radii[i]) * t)
    out_p.append(pts[-1])
    out_r.append(radii[-1])
    return out_p, out_r


class _Relief(Node):
    """Displaces the child's surface outward by Head.offset over the front of the face."""

    def __init__(self, child, head):
        self.child, self.head = child, head
        self.lo = child.lo - 0.03
        self.hi = child.hi + 0.03

    def eval(self, p):
        d, m = self.child.eval(p)
        h = self.head
        x, y, z = h.to_ref(p)
        front = np.clip((-0.02 - z) / 0.03, 0.0, 1.0)
        near = front > 0.0
        if near.any():
            xs, ys = x[near], y[near]
            e = 0.0004
            o = h.offset(xs, ys)
            gx = (h.offset(xs + e, ys) - h.offset(xs - e, ys)) / (2 * e)
            gy = (h.offset(xs, ys + e) - h.offset(xs, ys - e)) / (2 * e)
            fr = front[near]
            d[near] = (d[near] - o * fr * h.hs) / np.sqrt(1.0 + (gx * gx + gy * gy) * fr * fr)
        return d, m

    def leaves(self):
        return self.child.leaves()
