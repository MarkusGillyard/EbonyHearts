"""Crops the portrait screenshots the game renders with -SlopPortraits (head and shoulders in a
1920 x 1080 frame) to the select screen's 420 x 500 tiles, into SourceArt/Portraits/<id>.png.

    Tools/ue.sh: run_game -SlopPortraits[=id,id]   then   python3 -I Tools/portraits.py
"""
import glob
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS = os.path.join(HERE, "Saved", "Screenshots", "LinuxEditor")
OUT = os.path.join(HERE, "SourceArt", "Portraits")
W, H = 420, 500

os.makedirs(OUT, exist_ok=True)
done = 0
for path in sorted(glob.glob(os.path.join(SHOTS, "portrait_*.png"))):
    fid = os.path.basename(path)[len("portrait_"):-4]
    im = Image.open(path).convert("RGB")
    # The centre of the frame at the tile's aspect, full height.
    cw = round(im.height * W / H)
    x0 = (im.width - cw) // 2
    im = im.crop((x0, 0, x0 + cw, im.height)).resize((W, H), Image.LANCZOS)
    im.save(os.path.join(OUT, fid + ".png"), optimize=True)
    done += 1
    print(fid)
if not done:
    sys.exit("no portrait screenshots in " + SHOTS)
