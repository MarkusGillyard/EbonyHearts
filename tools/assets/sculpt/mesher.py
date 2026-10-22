"""Turns a signed distance field into a clean triangle mesh.

The field is sampled on a coarse grid everywhere, then block by block at the fine voxel size,
evaluating the exact field only near the surface. Marching cubes runs per block; blocks share
their boundary samples, so vertices weld exactly. The mesh is then decimated with quadric error
metrics and every vertex is projected back onto the exact surface, with normals taken from the
field's gradient rather than from the triangles.
"""
import time

import fast_simplification
import numpy as np
from scipy import ndimage
from skimage import measure

BATCH = 400_000


def evaluate(root, pts):
    d = np.empty(len(pts), np.float32)
    m = np.empty(len(pts), np.int32)
    for s in range(0, len(pts), BATCH):
        d[s:s + BATCH], m[s:s + BATCH] = root.eval(pts[s:s + BATCH])
    return d, m


def gradient(root, pts, eps=4e-4):
    g = np.empty((len(pts), 3), np.float32)
    for axis in range(3):
        off = np.zeros(3, np.float32)
        off[axis] = eps
        dp, _ = evaluate(root, pts + off)
        dn, _ = evaluate(root, pts - off)
        g[:, axis] = (dp - dn) / (2.0 * eps)
    return g


def polygonize(root, voxel, lo=None, hi=None, refine=4, block=64, log=print):
    t0 = time.time()
    lo = np.asarray(root.lo if lo is None else lo, np.float32) - voxel * 4
    hi = np.asarray(root.hi if hi is None else hi, np.float32) + voxel * 4
    coarse = voxel * refine
    n = np.ceil((hi - lo) / coarse).astype(int) + 1
    axes = [lo[i] + np.arange(n[i], dtype=np.float32) * coarse for i in range(3)]
    gx, gy, gz = np.meshgrid(*axes, indexing="ij")
    pts = np.stack([gx.ravel(), gy.ravel(), gz.ravel()], axis=1)
    dc, _ = evaluate(root, pts)
    dc = dc.reshape(n)
    log("  coarse grid %s (%d points) in %.1fs" % (tuple(n), dc.size, time.time() - t0))
    bc = block // refine
    verts, faces = [], []
    count = 0
    for bi in range(0, n[0] - 1, bc):
        for bj in range(0, n[1] - 1, bc):
            for bk in range(0, n[2] - 1, bc):
                sub = dc[bi:bi + bc + 1, bj:bj + bc + 1, bk:bk + bc + 1]
                if np.min(np.abs(sub)) > coarse * 2.5:
                    continue
                shape = (np.array(sub.shape) - 1) * refine + 1
                coords = np.indices(shape, dtype=np.float32) / refine
                fine = ndimage.map_coordinates(sub, coords, order=1)
                origin = lo + np.array([bi, bj, bk], np.float32) * coarse
                near = np.abs(fine) < coarse * 2.0
                if not near.any():
                    continue
                idx = np.argwhere(near).astype(np.float32)
                exact, _ = evaluate(root, origin + idx * voxel)
                fine[near] = exact
                if fine.min() >= 0.0 or fine.max() <= 0.0:
                    continue
                v, f, _, _ = measure.marching_cubes(fine, 0.0, spacing=(voxel, voxel, voxel), allow_degenerate=False)
                faces.append(f + count)
                verts.append(v.astype(np.float32) + origin)
                count += len(v)
    v = np.concatenate(verts)
    f = np.concatenate(faces)
    key = np.round(v / (voxel * 1e-3)).astype(np.int64)
    _, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    v = v[first]
    f = inv.ravel()[f]
    keep = (f[:, 0] != f[:, 1]) & (f[:, 1] != f[:, 2]) & (f[:, 0] != f[:, 2])
    f = f[keep]
    log("  marching cubes: %d vertices, %d triangles in %.1fs" % (len(v), len(f), time.time() - t0))
    return v, f


def orient_outward(root, v, f):
    """Flips the winding if triangle normals point into the field instead of out of it."""
    c = v[f].mean(axis=1)
    fn = np.cross(v[f[:, 1]] - v[f[:, 0]], v[f[:, 2]] - v[f[:, 0]])
    sample = np.random.default_rng(1).choice(len(f), min(4000, len(f)), replace=False)
    g = gradient(root, c[sample])
    if np.sum(np.einsum("ij,ij->i", fn[sample], g)) < 0.0:
        f = f[:, ::-1].copy()
    return f


class Focus:
    """A smooth radial magnification about `centre`: 1 + (scale - 1) inside r0, easing to 1 at
    r1. Decimating in the magnified space keeps more triangles where detail matters (the face).
    The radius r * g(r) must keep increasing, which a gentle scale over a wide falloff ensures."""

    def __init__(self, centre, r0, r1, scale):
        self.c = np.asarray(centre, np.float32)
        self.r0, self.r1, self.a = float(r0), float(r1), float(scale) - 1.0

    def _g(self, r):
        t = np.clip((self.r1 - r) / (self.r1 - self.r0), 0.0, 1.0)
        return 1.0 + self.a * t * t * (3 - 2 * t)

    def warp(self, v):
        d = v - self.c
        r = np.linalg.norm(d, axis=1)
        return self.c + d * self._g(r)[:, None]

    def unwarp(self, w):
        """Inverse of warp, by bisection on the radius (r * g(r) increases with r)."""
        d = w - self.c
        R = np.linalg.norm(d, axis=1)
        lo = np.zeros_like(R)
        hi = R.copy()
        for _ in range(40):
            mid = (lo + hi) * 0.5
            big = mid * self._g(mid) > R
            hi = np.where(big, mid, hi)
            lo = np.where(big, lo, mid)
        r = (lo + hi) * 0.5
        return self.c + d / np.maximum(R, 1e-9)[:, None] * r[:, None]


def simplify(v, f, target, log=print, focus=None):
    if len(f) <= target:
        return v, f
    t0 = time.time()
    src = focus.warp(v) if focus else v
    v2, f2 = fast_simplification.simplify(src, f, target_reduction=1.0 - target / len(f), agg=6)
    v2 = v2.astype(np.float32)
    if focus:
        v2 = focus.unwarp(v2).astype(np.float32)
    log("  decimated to %d triangles in %.1fs" % (len(f2), time.time() - t0))
    return v2, f2.astype(np.int64)


def project(root, v, iterations=3):
    """Moves vertices onto the zero set along the gradient."""
    for _ in range(iterations):
        d, _ = evaluate(root, v)
        g = gradient(root, v)
        gl2 = np.maximum(np.einsum("ij,ij->i", g, g), 1e-6)
        step = np.clip(d / gl2, -0.004, 0.004)
        v = v - g * step[:, None]
    return v


def normals(root, v):
    g = gradient(root, v)
    return g / np.maximum(np.linalg.norm(g, axis=1, keepdims=True), 1e-9)


def ambient_occlusion(root, v, nrm, steps=(0.003, 0.007, 0.013, 0.022, 0.035), strength=0.9):
    """SDF ambient occlusion: how much closer the surface gets than the distance stepped out."""
    occ = np.zeros(len(v), np.float32)
    total = 0.0
    for i, s in enumerate(steps):
        d, _ = evaluate(root, v + nrm * s)
        w = 0.5 ** i
        occ += w * np.clip((s - d) / s, 0.0, 1.0)
        total += w
    return np.clip(1.0 - strength * occ / total, 0.0, 1.0)


def adjacency(f, nv):
    from scipy import sparse
    i = np.concatenate([f[:, 0], f[:, 1], f[:, 2], f[:, 1], f[:, 2], f[:, 0]])
    j = np.concatenate([f[:, 1], f[:, 2], f[:, 0], f[:, 0], f[:, 1], f[:, 2]])
    a = sparse.coo_matrix((np.ones(len(i), np.float32), (i, j)), shape=(nv, nv)).tocsr()
    a.data[:] = 1.0
    deg = np.asarray(a.sum(axis=1)).ravel()
    return a, np.maximum(deg, 1.0)
