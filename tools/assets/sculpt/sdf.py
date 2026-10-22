"""Signed distance fields for sculpting characters.

Every node evaluates a batch of points (N x 3, float32) and returns distances plus the index of
the leaf primitive closest to each point, which later decides material and skin weights. Nodes
carry a bounding box so unions only evaluate children near the points that need them.
"""
import numpy as np

FAR = np.float32(10.0)


def _f(a):
    return np.asarray(a, dtype=np.float32)


class Node:
    lo = None
    hi = None

    def eval(self, p):
        raise NotImplementedError

    def leaves(self):
        return []


class Leaf(Node):
    """A primitive with a material key and the bones that may move it."""

    def __init__(self, fn, lo, hi, material, bones, tag=""):
        self.fn = fn
        self.lo = _f(lo)
        self.hi = _f(hi)
        self.material = material
        self.bones = list(bones)
        self.tag = tag
        self.index = -1

    def eval(self, p):
        return self.fn(p), np.full(len(p), self.index, dtype=np.int32)

    def leaves(self):
        return [self]


def _inside(p, lo, hi):
    return np.all((p >= lo) & (p <= hi), axis=1)


def smin(a, b, k):
    if k <= 0.0:
        return np.minimum(a, b)
    h = np.maximum(k - np.abs(a - b), 0.0) / k
    return np.minimum(a, b) - h * h * k * 0.25


def smax(a, b, k):
    return -smin(-a, -b, k)


class Union(Node):
    """Smooth union; `k` is the blend radius in metres (0 for a hard union)."""

    def __init__(self, children, k=0.0):
        self.children = [c for c in children if c is not None]
        self.k = float(k)
        pad = self.k * 1.5
        self.lo = np.min([c.lo for c in self.children], axis=0) - pad
        self.hi = np.max([c.hi for c in self.children], axis=0) + pad

    def eval(self, p):
        d = np.full(len(p), FAR, dtype=np.float32)
        m = np.full(len(p), -1, dtype=np.int32)
        pad = self.k * 1.5
        for c in self.children:
            idx = np.nonzero(_inside(p, c.lo - pad, c.hi + pad))[0]
            if len(idx) == 0:
                continue
            dc, mc = c.eval(p[idx])
            old = d[idx]
            closer = dc < old
            m[idx[closer]] = mc[closer]
            d[idx] = smin(old, dc, self.k)
        return d, m

    def leaves(self):
        out = []
        for c in self.children:
            out += c.leaves()
        return out


class Subtract(Node):
    """`a` with `b` carved out, blended over `k`. Cut faces keep a's material."""

    def __init__(self, a, b, k=0.0):
        self.a, self.b, self.k = a, b, float(k)
        self.lo, self.hi = a.lo, a.hi

    def eval(self, p):
        d, m = self.a.eval(p)
        idx = np.nonzero(_inside(p, self.b.lo - self.k, self.b.hi + self.k))[0]
        if len(idx):
            db, _ = self.b.eval(p[idx])
            d[idx] = smax(d[idx], -db, self.k)
        return d, m

    def leaves(self):
        return self.a.leaves() + self.b.leaves()


class Intersect(Node):
    def __init__(self, a, b, k=0.0):
        self.a, self.b, self.k = a, b, float(k)
        self.lo = np.maximum(a.lo, b.lo)
        self.hi = np.minimum(a.hi, b.hi)

    def eval(self, p):
        d, m = self.a.eval(p)
        db, _ = self.b.eval(p)
        return smax(d, db, self.k), m

    def leaves(self):
        return self.a.leaves() + self.b.leaves()


class Displace(Node):
    """Adds fn(p) to the child's distance (wrinkles, ribbing). `amp` bounds |fn|."""

    def __init__(self, child, fn, amp):
        self.child, self.fn = child, fn
        self.lo, self.hi = child.lo - amp, child.hi + amp

    def eval(self, p):
        d, m = self.child.eval(p)
        return d + self.fn(p).astype(np.float32), m

    def leaves(self):
        return self.child.leaves()


class Offset(Node):
    """The child grown outward by `r` (negative shrinks): a garment that follows any shape."""

    def __init__(self, child, r):
        self.child, self.r = child, float(r)
        self.lo, self.hi = child.lo - max(self.r, 0.0), child.hi + max(self.r, 0.0)

    def eval(self, p):
        d, m = self.child.eval(p)
        return d - self.r, m

    def leaves(self):
        return self.child.leaves()


class Shell(Node):
    """A hollow layer `t` thick on the child's surface (open garments: sleeves, hems)."""

    def __init__(self, child, t):
        self.child, self.t = child, float(t)
        self.lo, self.hi = child.lo - t, child.hi + t

    def eval(self, p):
        d, m = self.child.eval(p)
        return np.abs(d) - self.t, m

    def leaves(self):
        return self.child.leaves()


# --- Primitives --------------------------------------------------------------------------------

def frame_from(axis, ref):
    """Orthonormal frame (3 x 3, columns x, y, z) whose y is `axis` and whose z leans to `ref`."""
    y = _f(axis) / np.linalg.norm(axis)
    z = _f(ref) - y * np.dot(ref, y)
    if np.linalg.norm(z) < 1e-6:
        z = np.array([0, 0, 1], np.float32) - y * y[2]
    z = z / np.linalg.norm(z)
    x = np.cross(y, z)
    return np.stack([x, y, z], axis=1).astype(np.float32)


def round_cone(a, b, ra, rb, material, bones, squash=(1.0, 1.0), ref=(0, 0, 1), tag=""):
    """Capsule from a (radius ra) to b (radius rb). `squash` scales the cross-section along the
    frame's x and z (z leans toward `ref`), for flattened limbs."""
    a, b = _f(a), _f(b)
    ax = b - a
    length = float(np.linalg.norm(ax))
    if length < 1e-5:
        return sphere(a, max(ra, rb), material, bones, tag)
    rot = frame_from(ax, ref)
    sx, sz = float(squash[0]), float(squash[1])
    smin_s = min(sx, sz, 1.0)

    def fn(p):
        q = (p - a) @ rot
        q = q / np.array([sx, 1.0, sz], np.float32)
        return _round_cone_local(q, ra, rb, length) * smin_s

    r = max(ra, rb) * max(sx, sz, 1.0)
    lo = np.minimum(a, b) - r
    hi = np.maximum(a, b) + r
    return Leaf(fn, lo, hi, material, bones, tag)


def _round_cone_local(q, r1, r2, h):
    # Exact round cone along +y from 0 (radius r1) to h (radius r2) (after Inigo Quilez).
    if h < 1e-6:
        return np.linalg.norm(q, axis=1) - max(r1, r2)
    b = (r1 - r2) / h
    a = np.sqrt(max(1.0 - b * b, 1e-6))
    qx = np.sqrt(q[:, 0] ** 2 + q[:, 2] ** 2)
    qy = q[:, 1]
    k = -b * qx + a * qy
    d_mid = qx * a + qy * b - r1
    d_bot = np.sqrt(qx * qx + qy * qy) - r1
    d_top = np.sqrt(qx * qx + (qy - h) ** 2) - r2
    return np.where(k < 0.0, d_bot, np.where(k > a * h, d_top, d_mid)).astype(np.float32)


def ellipsoid(c, radii, material, bones, rot=None, tag=""):
    """Ellipsoid at c with semi-axes `radii` along the columns of `rot` (identity if None)."""
    c = _f(c)
    r = _f(radii)
    R = np.eye(3, dtype=np.float32) if rot is None else _f(rot)

    def fn(p):
        q = (p - c) @ R
        k0 = np.linalg.norm(q / r, axis=1)
        k1 = np.linalg.norm(q / (r * r), axis=1)
        return (k0 * (k0 - 1.0) / np.maximum(k1, 1e-9)).astype(np.float32)

    # The distance estimate undershoots away from elongated ellipsoids; pad the box so blends
    # never meet its edge.
    ext = np.abs(R) @ r + float(np.max(r)) * 0.6
    return Leaf(fn, c - ext, c + ext, material, bones, tag)


def sphere(c, radius, material, bones, tag=""):
    c = _f(c)

    def fn(p):
        return (np.linalg.norm(p - c, axis=1) - radius).astype(np.float32)

    return Leaf(fn, c - radius, c + radius, material, bones, tag)


def box(c, half, radius, material, bones, rot=None, tag=""):
    """Rounded box: half extents `half` (including the rounding) along the columns of `rot`."""
    c = _f(c)
    hb = _f(half) - radius
    R = np.eye(3, dtype=np.float32) if rot is None else _f(rot)

    def fn(p):
        q = np.abs((p - c) @ R) - hb
        out = np.linalg.norm(np.maximum(q, 0.0), axis=1)
        inside = np.minimum(np.max(q, axis=1), 0.0)
        return (out + inside - radius).astype(np.float32)

    ext = np.abs(R) @ _f(half)
    return Leaf(fn, c - ext, c + ext, material, bones, tag)


def torus(c, major, minor, material, bones, rot=None, squash=1.0, tag=""):
    """Torus around the frame's y axis; `squash` flattens it along y."""
    c = _f(c)
    R = np.eye(3, dtype=np.float32) if rot is None else _f(rot)

    def fn(p):
        q = (p - c) @ R
        qy = q[:, 1] / squash
        rxz = np.sqrt(q[:, 0] ** 2 + q[:, 2] ** 2) - major
        return ((np.sqrt(rxz * rxz + qy * qy) - minor) * min(squash, 1.0)).astype(np.float32)

    e = major + minor
    ext = np.abs(R) @ _f([e, minor * squash, e])
    return Leaf(fn, c - ext, c + ext, material, bones, tag)


def retag(node, material, bones, tag=""):
    """Any shape as a single primitive with its own material and bones (a cloth mask over the
    face, a garment grown from the body)."""

    def fn(p):
        return node.eval(p)[0]

    return Leaf(fn, node.lo, node.hi, material, bones, tag)


def cut(fn):
    """A region from an arbitrary function, negative where kept, for intersecting garments with.
    Its box covers everything, so an intersection keeps the garment's own bounds."""
    return Leaf(fn, [-4, -4, -4], [4, 4, 4], "", [], "cut")


def half_space(point, normal, size=4.0):
    """Everything on the side of the plane that `normal` points away from (d < 0 behind it)."""
    pt = _f(point)
    n = _f(normal) / np.linalg.norm(normal)

    def fn(p):
        return ((p - pt) @ n).astype(np.float32)

    return Leaf(fn, pt - size, pt + size, "", [], "cut")


def curve_tube(points, radii, material, bones, k=0.0, tag=""):
    """A tube through `points` with a radius per point, as a smooth chain of round cones."""
    parts = []
    for i in range(len(points) - 1):
        parts.append(round_cone(points[i], points[i + 1], radii[i], radii[i + 1], material, bones, tag=tag))
    return Union(parts, k) if len(parts) > 1 else parts[0]


# --- Noise -------------------------------------------------------------------------------------

def _hash3(ix, iy, iz, seed):
    h = (ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791) ^ (seed * 2654435761)
    h = (h ^ (h >> 13)) * 1274126177
    h = h ^ (h >> 16)
    return (h & 0xFFFF).astype(np.float32) / 65535.0


def value_noise(p, scale, seed=0):
    """Smooth value noise in [-1, 1] at `scale` features per metre."""
    q = p * scale
    i = np.floor(q).astype(np.int64)
    f = q - i
    f = f * f * (3.0 - 2.0 * f)
    out = np.zeros(len(p), np.float32)
    for dx in (0, 1):
        wx = f[:, 0] if dx else 1.0 - f[:, 0]
        for dy in (0, 1):
            wy = f[:, 1] if dy else 1.0 - f[:, 1]
            for dz in (0, 1):
                wz = f[:, 2] if dz else 1.0 - f[:, 2]
                out += wx * wy * wz * _hash3(i[:, 0] + dx, i[:, 1] + dy, i[:, 2] + dz, seed)
    return out * 2.0 - 1.0


def fbm(p, scale, octaves=3, seed=0):
    out = np.zeros(len(p), np.float32)
    amp, total = 1.0, 0.0
    for o in range(octaves):
        out += amp * value_noise(p, scale * (2.0 ** o), seed + o * 17)
        total += amp
        amp *= 0.5
    return out / total
