"""Measures a recording of the game's audio: speaker_stats.py <file.raw> <channels> <layout name>.

Raw float32le interleaved, as parec writes it. For each speaker: RMS and peak in dBFS, and how many
samples reached full scale, which is what clipping sounds like. Silence is reported as such, so a
speaker the mix never reaches stands out. Pure Python: no packages needed.
"""
import array
import math
import sys

NAMES = {1: ["M"], 2: ["FL", "FR"], 3: ["FL", "FR", "LFE"], 4: ["FL", "FR", "RL", "RR"],
         6: ["FL", "FR", "FC", "LFE", "RL", "RR"], 8: ["FL", "FR", "FC", "LFE", "RL", "RR", "SL", "SR"]}


def db(v):
    return 20.0 * math.log10(v) if v > 1e-7 else -140.0


def main():
    path, n, name = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    a = array.array("f")
    with open(path, "rb") as f:
        a.frombytes(f.read())
    # Whole frames only, and skip the first second, which is the stream opening.
    frames = len(a) // n
    start = 48000
    if frames <= start:
        print(f"{name}: no audio recorded")
        return 1
    out = []
    worst = -140.0
    clipped = 0
    for c in range(n):
        s = a[start * n + c:frames * n:n]
        e = sum(x * x for x in s) / max(1, len(s))
        p = max((abs(x) for x in s), default=0.0)
        o = sum(1 for x in s if abs(x) >= 0.999)
        clipped += o
        worst = max(worst, db(p))
        label = NAMES.get(n, [str(i) for i in range(n)])[c]
        out.append(f"{label} {db(math.sqrt(e)):6.1f}/{db(p):5.1f}" if e > 1e-12 else f"{label}  silent")
    print(f"{name:7s} {(frames - start) / 48000:4.0f}s  peak {worst:5.1f} dBFS  clipped {clipped:6d}  rms/peak: " + "  ".join(out))
    return 0


sys.exit(main())
