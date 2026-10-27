"""Tiles screenshots into labelled contact sheets for review: sheet.py <out.png> <cols> <crop:x0,y0,x1,y1|none> <images...>"""
import os, sys
from PIL import Image, ImageDraw
out, cols, crop = sys.argv[1], int(sys.argv[2]), sys.argv[3]
files = sys.argv[4:]
box = None if crop == "none" else tuple(int(v) for v in crop.split(","))
tiles = []
for f in files:
    im = Image.open(f).convert("RGB")
    if box:
        im = im.crop(box)
    im.thumbnail((640, 640))
    d = ImageDraw.Draw(im)
    label = os.path.basename(f).replace(".png", "")
    d.rectangle([0, 0, im.width, 18], fill=(0, 0, 0))
    d.text((4, 3), label, fill=(255, 255, 255))
    tiles.append(im)
w = max(t.width for t in tiles); h = max(t.height for t in tiles)
rows = (len(tiles) + cols - 1) // cols
sheet = Image.new("RGB", (w * cols, h * rows), (20, 20, 20))
for i, t in enumerate(tiles):
    sheet.paste(t, ((i % cols) * w, (i // cols) * h))
sheet.save(out)
print(out, sheet.size)
