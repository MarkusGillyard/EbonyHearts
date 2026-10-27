"""Prepares the CC0 surface sets for import: one colour map, one DirectX normal map and one packed
ORM map (R ambient occlusion, G roughness, B metallic) per set, with stable names.

    /home/cinqic/Documents/Mortal Slopbat/.venv/bin/python -I Tools/prep_surfaces.py

Reads SourceArt/ThirdParty/{ambientCG,PolyHaven/textures} (see ledger.json for provenance) and
writes SourceArt/Surfaces/<Set>/<Set>_{basecolor,normal,orm}.png. The four fighting floors keep
2048 px; every other set, seen smaller and further off, is reduced to 1024 px. Nothing else about
the images is changed.
"""
import glob
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
TP = os.path.join(ROOT, "SourceArt", "ThirdParty")
OUT = os.path.join(ROOT, "SourceArt", "Surfaces")
FLOORS = {"MetalPlates013", "monastery_stone_floor", "weathered_planks", "Rock035"}
MAX = 1024


def load(path, mode):
    im = Image.open(path).convert(mode)
    if max(im.size) > MAX:
        k = MAX / max(im.size)
        im = im.resize((int(im.size[0] * k), int(im.size[1] * k)), Image.LANCZOS)
    return im


def write(set_id, color, normal, ao, rough, metal):
    global MAX
    MAX = 2048 if set_id in FLOORS else 1024
    d = os.path.join(OUT, set_id)
    os.makedirs(d, exist_ok=True)
    c = load(color, "RGB")
    c.save(os.path.join(d, f"{set_id}_basecolor.png"))
    if normal:
        load(normal, "RGB").resize(c.size, Image.LANCZOS).save(os.path.join(d, f"{set_id}_normal.png"))
    size = c.size
    def chan(p, default):
        if p is None:
            return np.full((size[1], size[0]), default, np.uint8)
        return np.asarray(load(p, "L").resize(size, Image.LANCZOS))
    orm = np.stack([chan(ao, 255), chan(rough, 200), chan(metal, 0)], axis=-1)
    Image.fromarray(orm, "RGB").save(os.path.join(d, f"{set_id}_orm.png"))
    print(f"{set_id}: {size[0]}x{size[1]}  normal={'yes' if normal else 'no'} ao={'yes' if ao else 'no'} metal={'yes' if metal else 'no'}")


def first(pattern):
    m = sorted(glob.glob(pattern))
    return m[0] if m else None


def main():
    only = set(sys.argv[1:])
    for d in sorted(glob.glob(os.path.join(TP, "ambientCG", "*", "*-JPG"))):
        set_id = os.path.basename(os.path.dirname(d))
        if only and set_id not in only:
            continue
        color = first(os.path.join(d, "*_Color.jpg"))
        if not color:
            continue
        write(set_id, color, first(os.path.join(d, "*_NormalDX.jpg")), first(os.path.join(d, "*_AmbientOcclusion.jpg")),
              first(os.path.join(d, "*_Roughness.jpg")), first(os.path.join(d, "*_Metalness.jpg")))
    for d in sorted(glob.glob(os.path.join(TP, "PolyHaven", "textures", "*", "*_jpg"))):
        set_id = os.path.basename(os.path.dirname(d))
        if only and set_id not in only:
            continue
        color = first(os.path.join(d, "*_diff_*.jpg"))
        if not color:
            continue
        arm = first(os.path.join(d, "*_arm_*.jpg"))
        if arm:
            # Poly Haven's ARM is already occlusion, roughness, metal.
            dd = os.path.join(OUT, set_id)
            os.makedirs(dd, exist_ok=True)
            write(set_id, color, first(os.path.join(d, "*_nor_dx_*.jpg")), None, None, None)
            a = load(arm, "RGB").resize(Image.open(os.path.join(dd, f"{set_id}_basecolor.png")).size, Image.LANCZOS)
            a.save(os.path.join(dd, f"{set_id}_orm.png"))
        else:
            write(set_id, color, first(os.path.join(d, "*_nor_dx_*.jpg")), first(os.path.join(d, "*_ao_*.jpg")),
                  first(os.path.join(d, "*_rough_*.jpg")), None)


main()
