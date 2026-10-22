"""Skin weights: each vertex is shared between the bones its primitive allows, by distance to
the bones, then smoothed across the surface so joints bend without creases."""
import numpy as np

from mesher import adjacency


def segment_distance(p, a, b):
    ab = b - a
    l2 = max(float(np.dot(ab, ab)), 1e-12)
    t = np.clip((p - a) @ ab / l2, 0.0, 1.0)
    return np.linalg.norm(p - (a + t[:, None] * ab), axis=1)


def compute(v, f, allowed, segments, bone_index, smooth_iters=10, max_influences=4):
    """`allowed[i]` lists the bone names vertex i may follow; `segments[name]` is (a, b)."""
    nb = len(bone_index)
    w = np.zeros((len(v), nb), np.float32)
    groups = {}
    for i, names in enumerate(allowed):
        groups.setdefault(tuple(names), []).append(i)
    for names, idx in groups.items():
        idx = np.array(idx)
        pts = v[idx]
        dist = np.stack([segment_distance(pts, *segments[n]) for n in names], axis=1)
        inv = 1.0 / np.maximum(dist, 0.004) ** 4
        inv /= inv.sum(axis=1, keepdims=True)
        for k, n in enumerate(names):
            w[idx, bone_index[n]] += inv[:, k]
    adj, deg = adjacency(f, len(v))
    for _ in range(smooth_iters):
        w = 0.5 * w + 0.5 * (adj @ w) / deg[:, None]
    top = np.argsort(-w, axis=1)[:, :max_influences]
    tw = np.take_along_axis(w, top, axis=1)
    tw = np.where(tw < 0.01, 0.0, tw)
    tw /= np.maximum(tw.sum(axis=1, keepdims=True), 1e-9)
    return top.astype(np.int32), tw.astype(np.float32)
