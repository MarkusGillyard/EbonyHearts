"""Ray-marches a fighter's sculpt (the distance field itself, before meshing) from a few views
into one PNG, for iterating on shapes quickly.

    .venv/bin/python tools/assets/sculpt/preview.py daxion --part=head --out=/tmp/head.png
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))

import mesher  # noqa: E402
import outfits  # noqa: E402
from human import Body  # noqa: E402
from rig import Rig  # noqa: E402
from sdf import Union, sphere  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))


def render(node, centre, dist, yaw_deg, size=360, fov=30.0, colour=None):
    yaw = np.radians(yaw_deg)
    fwd = np.array([np.sin(yaw), 0.0, -np.cos(yaw)], np.float32)  # camera looks along -fwd
    cam = centre + fwd * dist
    look = -fwd
    right = np.cross(look, [0, 1, 0]).astype(np.float32)
    right /= np.linalg.norm(right)
    up = np.cross(right, look)
    t = np.tan(np.radians(fov) * 0.5)
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float32)
    u = (xs / size - 0.5) * 2 * t
    v = -(ys / size - 0.5) * 2 * t
    dirs = look[None, :] + u.reshape(-1, 1) * right[None, :] + v.reshape(-1, 1) * up[None, :]
    dirs /= np.linalg.norm(dirs, axis=1, keepdims=True)
    depth = np.full(len(dirs), max(dist - 0.6, 0.05), np.float32)
    alive = np.ones(len(dirs), bool)
    hit = np.zeros(len(dirs), bool)
    for _ in range(160):
        idx = np.nonzero(alive)[0]
        if len(idx) == 0:
            break
        p = cam + dirs[idx] * depth[idx, None]
        d, _ = mesher.evaluate(node, p)
        h = d < 0.0004
        hit[idx[h]] = True
        alive[idx[h]] = False
        depth[idx] += np.clip(d, 0.0004, 0.03) * 0.9
        far = depth[idx] > dist + 0.8
        alive[idx[far]] = False
    img = np.zeros((len(dirs), 3), np.float32) + np.array([0.12, 0.13, 0.16])
    hi = np.nonzero(hit)[0]
    if len(hi):
        p = cam + dirs[hi] * depth[hi, None]
        n = mesher.normals(node, p)
        light = np.array([-0.4, 0.6, -0.7], np.float32)
        light /= np.linalg.norm(light)
        diff = np.clip(n @ light, 0, 1)
        rim = np.clip(n @ np.array([0.7, 0.2, 0.6]), 0, 1) * 0.25
        _, m = mesher.evaluate(node, p)
        base = np.array([0.8, 0.62, 0.5], np.float32)
        ao = mesher.ambient_occlusion(node, p, n)
        img[hi] = base * (0.18 + 0.82 * diff[:, None]) * ao[:, None] + rim[:, None]
    return (np.clip(img.reshape(size, size, 3), 0, 1) ** (1 / 2.2) * 255).astype(np.uint8)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("fighter")
    ap.add_argument("--part", default="head")
    ap.add_argument("--yaws", default="0,35,90")
    ap.add_argument("--size", type=int, default=360)
    ap.add_argument("--out", default="/tmp/sculpt.png")
    a = ap.parse_args()
    rig = Rig(os.path.join(ROOT, "build", "characters", a.fighter + ".rig.json"))
    b = Body(rig)
    if a.part == "head":
        eyes, er = b.eye_centres()
        node = Union([b.head(), b.neck()] + [sphere(c, er, "eye", ["head"]) for c in eyes], 0.0)
        centre, dist = rig.pos("head") + np.array([0, 0.09 * b.s, -0.02], np.float32), 0.55
    elif a.part == "face":
        eyes, er = b.eye_centres()
        node = Union([b.head(), b.neck()] + [sphere(c, er, "eye", ["head"]) for c in eyes], 0.0)
        centre, dist = rig.pos("head") + np.array([0, 0.07 * b.s, -0.05], np.float32), 0.36
    elif a.part == "hand":
        node = b.hand("l")[0]
        centre, dist = rig.at("hand_l", [0, -0.07 * b.s, 0]), 0.35
    elif a.part == "upper":
        node = outfits.for_fighter(rig).body(b)
        centre, dist = np.array([0, rig.h * 0.8, 0], np.float32), 1.3
    else:
        node = outfits.for_fighter(rig).body(b)
        centre, dist = np.array([0, rig.h * 0.52, 0], np.float32), 3.6
    for i, lf in enumerate(node.leaves()):
        lf.index = i
    shots = [render(node, centre, dist, float(y), a.size) for y in a.yaws.split(",")]
    Image.fromarray(np.concatenate(shots, axis=1)).save(a.out)
    print("wrote", a.out)


if __name__ == "__main__":
    main()
