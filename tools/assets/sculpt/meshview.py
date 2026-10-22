"""Renders the meshes stored in a .wfm file in their bind pose with a small z-buffer rasterizer
(both faces drawn), to check what meshing and decimation did to a sculpt.

    .venv/bin/python tools/assets/sculpt/meshview.py daxion --centre=0,1.83,-0.05 --dist=0.32 --out=/tmp/m.png
"""
import argparse
import json
import os
import struct
import zlib

import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))


def load(path):
    data = open(path, "rb").read()
    hl, cl = struct.unpack("<II", data[4:12])
    header = json.loads(data[12:12 + hl])
    payload = zlib.decompress(data[12 + hl:12 + hl + cl])

    def arr(m, k, dt, w):
        a = m["arrays"][k]
        return np.frombuffer(payload[a["offset"]:a["offset"] + a["bytes"]], dtype=dt).reshape(-1, w)

    return header, arr


def render(meshes, centre, dist, yaw, size):
    yaw = np.radians(yaw)
    fwd = np.array([np.sin(yaw), 0.0, -np.cos(yaw)])
    cam = centre + fwd * dist
    look = -fwd
    right = np.cross(look, [0, 1, 0])
    right /= np.linalg.norm(right)
    up = np.cross(right, look)
    f = size / (2 * np.tan(np.radians(15)))
    img = np.full((size, size, 3), 0.12)
    zbuf = np.full((size, size), np.inf)
    light = np.array([-0.4, 0.6, -0.7])
    light /= np.linalg.norm(light)
    for v, n, idx, col in meshes:
        rel = v - cam
        z = rel @ look
        x = rel @ right / np.maximum(z, 1e-4) * f + size / 2
        y = -(rel @ up) / np.maximum(z, 1e-4) * f + size / 2
        shade = np.clip(n @ light, 0, 1) * 0.8 + 0.2
        tri = idx.reshape(-1, 3)
        keep = np.all(z[tri] > 0.01, axis=1)
        for t in tri[keep]:
            xs, ys, zs = x[t], y[t], z[t]
            x0, x1 = int(max(np.floor(xs.min()), 0)), int(min(np.ceil(xs.max()), size - 1))
            y0, y1 = int(max(np.floor(ys.min()), 0)), int(min(np.ceil(ys.max()), size - 1))
            if x0 > x1 or y0 > y1:
                continue
            gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            d = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
            if abs(d) < 1e-12:
                continue
            a = ((ys[1] - ys[2]) * (gx - xs[2]) + (xs[2] - xs[1]) * (gy - ys[2])) / d
            b = ((ys[2] - ys[0]) * (gx - xs[2]) + (xs[0] - xs[2]) * (gy - ys[2])) / d
            c = 1 - a - b
            inside = (a >= 0) & (b >= 0) & (c >= 0)
            if not inside.any():
                continue
            zz = a * zs[0] + b * zs[1] + c * zs[2]
            sh = a * shade[t[0]] + b * shade[t[1]] + c * shade[t[2]]
            sub = zbuf[y0:y1 + 1, x0:x1 + 1]
            win = inside & (zz < sub)
            sub[win] = zz[win]
            img[y0:y1 + 1, x0:x1 + 1][win] = np.asarray(col)[None, :] * sh[win][:, None]
    return (np.clip(img, 0, 1) ** (1 / 2.2) * 255).astype(np.uint8)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("fighter")
    ap.add_argument("--centre", default="0,1.6,0")
    ap.add_argument("--dist", type=float, default=0.5)
    ap.add_argument("--yaws", default="0,40,90")
    ap.add_argument("--size", type=int, default=360)
    ap.add_argument("--out", default="/tmp/meshview.png")
    a = ap.parse_args()
    header, arr = load(os.path.join(ROOT, "assets", "characters", a.fighter + ".wfm"))
    centre = np.array([float(x) for x in a.centre.split(",")])
    meshes = []
    colours = {"body": (0.8, 0.62, 0.5), "eye": (0.9, 0.9, 0.95)}
    for m in header["meshes"]:
        if m["skeleton"] != "body" or m["surfaces"][0]["material"] not in colours:
            continue
        meshes.append((arr(m, "vertex", np.float32, 3).astype(np.float64), arr(m, "normal", np.float32, 3).astype(np.float64),
                       arr(m, "index", np.int32, 1).ravel(), colours[m["surfaces"][0]["material"]]))
    shots = [render(meshes, centre, a.dist, float(y), a.size) for y in a.yaws.split(",")]
    Image.fromarray(np.concatenate(shots, axis=1)).save(a.out)
    print("wrote", a.out)


if __name__ == "__main__":
    main()
