"""Peak and loudness of the game's sounds, to keep the mix even without a listening session.

    python3 -I Tools/audio_levels.py            # every file
    python3 -I Tools/audio_levels.py impact     # names containing "impact"

Reads SourceArt/Audio/Original (WAV) and the Kenney packs in SourceArt/ThirdParty/Kenney (Ogg,
decoded to WAV with GStreamer into a temporary folder). Prints, per file, the peak and the RMS of
its loudest 100 ms in dBFS, then the same averaged per Kenney family (name up to the last _NNN):
the game plays a family's variants at random, so the family level is the one that matters. The
Kenney files are all peak-normalised to about -0.9 dB, so their RMS differs by how sharp they are.
"""
import array
import math
import os
import shutil
import subprocess
import sys
import tempfile
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def measure(path):
    with wave.open(path) as w:
        ch, sw, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if sw != 2 or n == 0:
        return None
    a = array.array("h", raw)
    if ch > 1:
        a = array.array("h", a[0::ch])
    peak = max(abs(x) for x in a) / 32768.0
    win = max(1, rate // 10)
    best = 0.0
    for i in range(0, max(1, len(a) - win), max(1, win // 4)):
        best = max(best, sum(x * x for x in a[i:i + win]) / win)
    db = lambda v: 20 * math.log10(v) if v > 0 else -120.0
    return db(peak), db(math.sqrt(best) / 32768.0), len(a) / rate


def main():
    want = sys.argv[1] if len(sys.argv) > 1 else ""
    files = []
    orig = os.path.join(ROOT, "SourceArt", "Audio", "Original")
    files += [(f, os.path.join(orig, f)) for f in sorted(os.listdir(orig)) if f.endswith(".wav")]
    tmp = tempfile.mkdtemp(prefix="slop_levels_")
    kenney = os.path.join(ROOT, "SourceArt", "ThirdParty", "Kenney")
    for d, _, fs in sorted(os.walk(kenney)):
        for f in sorted(fs):
            if f.endswith(".ogg") and want in f:
                out = os.path.join(tmp, f[:-4] + ".wav")
                subprocess.run(["gst-launch-1.0", "-q", "filesrc", "location=" + os.path.join(d, f), "!", "oggdemux", "!", "vorbisdec", "!",
                                "audioconvert", "!", "audio/x-raw,format=S16LE", "!", "wavenc", "!", "filesink", "location=" + out],
                               check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                if os.path.exists(out):
                    files.append(("kenney:" + f[:-4], out))
    families = {}
    print(" peak    rms   secs  name")
    for name, path in files:
        if want not in name:
            continue
        m = measure(path)
        if not m:
            continue
        print(f"{m[0]:5.1f} {m[1]:6.1f} {m[2]:6.2f}  {name}")
        if name.startswith("kenney:"):
            families.setdefault(name.rsplit("_", 1)[0], []).append(m)
    shutil.rmtree(tmp, ignore_errors=True)
    print("\nKenney families (mean peak, mean rms):")
    for fam, ms in sorted(families.items(), key=lambda kv: sum(m[1] for m in kv[1]) / len(kv[1])):
        print(f"{sum(m[0] for m in ms) / len(ms):5.1f} {sum(m[1] for m in ms) / len(ms):6.1f}  {fam} ({len(ms)})")


if __name__ == "__main__":
    main()
