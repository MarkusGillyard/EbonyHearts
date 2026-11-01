"""Resizes the stage renders the game makes with -SlopHideFighters -SlopShotsNoUi (each ground
empty, from the fight camera at the start of a round) to the stage screen's picture, into
SourceArt/StagePictures/<id>.png. The picture fills 1080 units, so this size is one to one at 4K.

    for each stage:  RESX=3840 RESY=2160 SLOP_OFFSCREEN=1 run_game -SlopMatch=ashka,bram,training,<stage> \\
                       -SlopHideFighters -SlopShotsNoUi -SlopShots=150 -SlopShotPrefix=stagepic_<stage> -SlopQuitAt=170
                     (a profile with dynamic_res off, or the render scale softens the picture)
    then:            python3 -I Tools/stage_pictures.py   (and the import step "stagepics")
"""
import glob
import os
import re
import sys

from PIL import Image

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS = os.path.join(HERE, "Saved", "Screenshots", "LinuxEditor")
OUT = os.path.join(HERE, "SourceArt", "StagePictures")
W, H = 2160, 1215

os.makedirs(OUT, exist_ok=True)
done = 0
for path in sorted(glob.glob(os.path.join(SHOTS, "stagepic_*.png"))):
    m = re.match(r"stagepic_(.+)_\d+\.png$", os.path.basename(path))
    if not m:
        continue
    sid = m.group(1)
    im = Image.open(path).convert("RGB")
    # The middle of the frame at 16:9, full width.
    ch = round(im.width * H / W)
    y0 = max(0, (im.height - ch) // 2)
    im = im.crop((0, y0, im.width, y0 + min(ch, im.height))).resize((W, H), Image.LANCZOS)
    im.save(os.path.join(OUT, sid + ".png"), optimize=True)
    done += 1
    print(sid)
if not done:
    sys.exit("no stage renders in " + SHOTS)
