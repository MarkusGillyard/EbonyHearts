"""Meshes that ride on a simulated cloth grid (tools/assets/export_rigs.gd exports each grid's
rest points in its anchor joint's frame).

Every grid point is a bone. A vertex follows two neighbouring columns linearly and four rows
through a clamped cubic B-spline (eight influences), with its rest position anywhere: on a smooth
surface through the points, pushed out along the normal for thickness and folds. The game
computes each point's frame the same way as `frames` below.
"""
import numpy as np


def frames(P, rows, cols, closed):
    """Bone frames (rows*cols, 4 x 4) from grid points: -y down the column, x along the row."""
    P = P.reshape(rows, cols, 3)
    out = np.zeros((rows, cols, 4, 4), np.float32)
    for r in range(rows):
        for c in range(cols):
            down = P[min(r + 1, rows - 1), c] - P[max(r - 1, 0), c]
            if closed:
                side = P[r, (c + 1) % cols] - P[r, (c - 1) % cols]
            else:
                side = P[r, min(c + 1, cols - 1)] - P[r, max(c - 1, 0)]
            y = -down / max(np.linalg.norm(down), 1e-9)
            x = side - y * np.dot(side, y)
            x /= max(np.linalg.norm(x), 1e-9)
            z = np.cross(x, y)
            out[r, c, :3, 0] = x
            out[r, c, :3, 1] = y
            out[r, c, :3, 2] = z
            out[r, c, :3, 3] = P[r, c]
            out[r, c, 3, 3] = 1.0
    return out.reshape(rows * cols, 4, 4)


def bspline_basis(n, t):
    """Clamped uniform cubic B-spline over n control points at t in [0, 1]: (first index, 4 weights)."""
    deg = 3
    inner = n - deg
    knots = np.concatenate([np.zeros(deg), np.linspace(0, 1, inner + 1), np.ones(deg)])
    t = min(max(t, 0.0), 1.0 - 1e-9)
    span = np.searchsorted(knots, t, side="right") - 1
    span = min(max(span, deg), n - 1)
    N = np.zeros(deg + 1)
    N[0] = 1.0
    left = np.zeros(deg + 1)
    right = np.zeros(deg + 1)
    for j in range(1, deg + 1):
        left[j] = t - knots[span + 1 - j]
        right[j] = knots[span + j] - t
        saved = 0.0
        for r in range(j):
            tmp = N[r] / (right[r + 1] + left[j - r])
            N[r] = saved + right[r + 1] * tmp
            saved = left[j - r] * tmp
        N[j] = saved
    return span - deg, N


def catmull(p0, p1, p2, p3, t):
    t2, t3 = t * t, t * t * t
    return 0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3)


class Grid:
    def __init__(self, spec):
        self.rows = int(spec["rows"])
        self.cols = int(spec["cols"])
        self.closed = bool(spec["closed"])
        self.P = np.array(spec["points"], np.float32).reshape(self.rows, self.cols, 3)
        self.spec = spec["spec"]
        self.joint = spec["joint"]
        self.binds = frames(self.P, self.rows, self.cols, self.closed)

    def _col(self, c):
        return c % self.cols if self.closed else min(max(c, 0), self.cols - 1)

    def surface(self, u, v):
        """Smooth point through the grid at column parameter u, row parameter v."""
        rows = []
        c1 = int(np.floor(u))
        fu = u - c1
        for r in (int(np.floor(v)) - 1, int(np.floor(v)), int(np.floor(v)) + 1, int(np.floor(v)) + 2):
            rr = min(max(r, 0), self.rows - 1)
            q = [self.P[rr, self._col(c1 + k)] for k in (-1, 0, 1, 2)]
            rows.append(catmull(q[0], q[1], q[2], q[3], fu))
        fv = v - np.floor(v)
        return catmull(rows[0], rows[1], rows[2], rows[3], fv)

    def frame(self, u, v, e=0.02):
        p = self.surface(u, v)
        du = self.surface(u + e, v) - self.surface(u - e, v)
        dv = self.surface(u, min(v + e, self.rows - 1)) - self.surface(u, max(v - e, 0))
        n = np.cross(du, dv)
        n /= max(np.linalg.norm(n), 1e-9)
        return p, du / max(np.linalg.norm(du), 1e-9), dv / max(np.linalg.norm(dv), 1e-9), n

    def weights(self, u, v):
        """Eight (bone, weight) pairs for parameters u, v."""
        c0 = int(np.floor(u))
        fu = u - c0
        cols = [(self._col(c0), 1.0 - fu), (self._col(c0 + 1), fu)]
        first, N = bspline_basis(self.rows, v / (self.rows - 1))
        bones, w = [], []
        for k in range(4):
            r = first + k
            for c, cw in cols:
                bones.append(r * self.cols + c)
                w.append(N[k] * cw)
        return bones, w


def hem_line(nu, tatter, rng, curve=0.0):
    """Where the hem ends in each column, as a fraction of the sheet's length: 1 for a straight
    hem, shorter toward an open sheet's edges by `curve` (veils, coat fronts), ragged points and
    torn notches for `tatter` > 0."""
    x = np.linspace(0.0, 1.0, nu)
    base = 1.0 - curve * (1.0 - np.sin(np.pi * x)) ** 1.5
    if tatter <= 0.0:
        return base
    teeth = max(3, nu // 7)
    phase = rng.uniform(0, 1, teeth + 2)
    depth = rng.uniform(0.35, 1.0, teeth + 2)
    k = x * teeth
    i = np.floor(k).astype(int)
    f = k - i
    # Each tooth is a ragged point: deepest at its edges, longest a little off its centre.
    tooth = np.abs(f - 0.5 - (phase[i] - 0.5) * 0.4) * 2.0
    cut = tatter * depth[i] * np.clip(tooth, 0.0, 1.0) ** 0.7
    cut += tatter * 0.12 * np.sin(x * 37.0 + phase[0] * 6.0)
    return np.clip(base - cut, 0.35, 1.0)


def skirt(grid, thickness, folds, fold_count, sub_u=6, sub_v=6, seed=0, tatter=0.0, curve=0.0):
    """A skirt or coat tail on the grid: an outer and an inner layer with a hem between them, and
    folds that deepen toward the hem. Open grids get finished side edges; `tatter` tears the hem
    into ragged points."""
    rng = np.random.default_rng(seed)
    cols, rows = grid.cols, grid.rows
    umax = cols if grid.closed else cols - 1
    nu = umax * sub_u + (0 if grid.closed else 1)
    nv = (rows - 1) * sub_v + 1
    phase = rng.uniform(0, 2 * np.pi)
    jitter = rng.uniform(0.7, 1.3, fold_count + 1)
    hem = hem_line(nu, tatter, rng, 0.0 if grid.closed else curve)
    pos = np.zeros((2, nv, nu, 3), np.float32)
    nrm = np.zeros((2, nv, nu, 3), np.float32)
    bw = []
    for j in range(nv):
        for i in range(nu):
            v = j / sub_v * hem[i]
            depth = (v / (rows - 1)) ** 1.3
            u = i / sub_u
            p, tu, tv, n = grid.frame(u, v)
            k = u / umax * fold_count
            amp = folds * depth * jitter[int(k) % fold_count]
            fold = amp * np.sin(k * 2 * np.pi + phase)
            # Folds tilt the surface as well as pushing it out.
            slope = amp * np.cos(k * 2 * np.pi + phase) * 2 * np.pi * fold_count / max(np.linalg.norm(grid.surface(u + 0.5, v) - grid.surface(u - 0.5, v)) * umax, 1e-6)
            nn = n - tu * slope
            nn /= np.linalg.norm(nn)
            for side, sgn in ((0, 1.0), (1, -1.0)):
                pos[side, j, i] = p + n * (fold + sgn * thickness * 0.5)
                nrm[side, j, i] = nn * sgn
            bw.append(grid.weights(u, v))
    verts, norms, bones, wts, idx = [], [], [], [], []

    def vid(side, j, i):
        return (side * nv + j) * nu + i

    for side in (0, 1):
        for j in range(nv):
            for i in range(nu):
                verts.append(pos[side, j, i])
                norms.append(nrm[side, j, i])
                b, w = bw[j * nu + i]
                bones.append(b)
                wts.append(w)
    wrap = grid.closed
    for side in (0, 1):
        for j in range(nv - 1):
            for i in range(nu if wrap else nu - 1):
                i2 = (i + 1) % nu
                a, b_, c, d = vid(side, j, i), vid(side, j, i2), vid(side, j + 1, i), vid(side, j + 1, i2)
                if side == 0:
                    idx += [a, c, b_, b_, c, d]
                else:
                    idx += [a, b_, c, b_, d, c]

    def strip(pairs):
        """Joins the two layers along an edge with its own vertices facing `outward`."""
        base = len(verts)
        for (j, i), n_out in pairs:
            for side in (0, 1):
                verts.append(pos[side, j, i])
                norms.append(n_out)
                b, w = bw[j * nu + i]
                bones.append(b)
                wts.append(w)
        for k in range(len(pairs) - 1):
            a, b_ = base + 2 * k, base + 2 * k + 1
            c, d = base + 2 * (k + 1), base + 2 * (k + 1) + 1
            idx.extend([a, b_, c, b_, d, c])

    edge = []
    for i in list(range(nu)) + ([0] if wrap else []):
        p, tu, tv, n = grid.frame(i / sub_u, (rows - 1) * hem[i])
        edge.append(((nv - 1, i), tv))
    strip(edge)
    if not wrap:
        for i, sgn in ((0, -1.0), (nu - 1, 1.0)):
            edge = []
            for j in range(nv):
                p, tu, tv, n = grid.frame(i / sub_u, j / sub_v * hem[i])
                edge.append(((j, i), tu * sgn))
            strip(edge)
    v = np.array(verts, np.float32)
    n = np.array(norms, np.float32)
    return v, n, np.array(bones, np.int32), np.array(wts, np.float32), orient(v, n, np.array(idx, np.int32))


def orient(v, n, idx):
    """Winds every triangle so it faces the way its vertex normals point."""
    f = idx.reshape(-1, 3).copy()
    fn = np.cross(v[f[:, 1]] - v[f[:, 0]], v[f[:, 2]] - v[f[:, 0]])
    flip = np.einsum("ij,ij->i", fn, n[f].sum(axis=1)) < 0
    f[flip] = f[flip][:, ::-1]
    return f.reshape(-1)
