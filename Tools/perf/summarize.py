"""Summarises a frame-stats CSV written by the game's -SlopFrameStats flag.

    python3 -I Tools/perf/summarize.py <framestats.csv> [--warmup 5] [--hitch 25]

For the whole run and for each scene (stage and fighters) it prints frame time median, p95,
p99 and maximum; average FPS; the 1% low (1000 / mean of the slowest 1% of frame times); hitches
(frames over the threshold, default 25 ms, i.e. a 60 Hz frame missed by more than half); the
game-thread, render-thread and GPU medians and p95; peak resident memory; how evenly frames are
paced (the share within 1 ms of the median); and the render scale dynamic resolution chose. The first
`--warmup` seconds of each match (loading, first-use shader compilation) are reported apart.
"""
import csv
import statistics
import sys


def pct(v, p):
    if not v:
        return float("nan")
    s = sorted(v)
    k = (len(s) - 1) * p / 100.0
    lo = int(k)
    hi = min(lo + 1, len(s) - 1)
    return s[lo] + (s[hi] - s[lo]) * (k - lo)


def one_percent_low(frames):
    s = sorted(frames, reverse=True)
    n = max(1, len(s) // 100)
    return 1000.0 / statistics.mean(s[:n])


def report(name, rows, hitch):
    if not rows:
        return
    f = [r["frame_ms"] for r in rows]
    g = [r["game_ms"] for r in rows]
    rt = [r["render_ms"] for r in rows]
    gpu = [r["gpu_ms"] for r in rows]
    secs = sum(f) / 1000.0
    h = sum(1 for x in f if x > hitch)
    print(f"{name}: {len(f)} frames, {secs:.0f} s")
    print(f"  frame ms  median {pct(f, 50):.2f}  p95 {pct(f, 95):.2f}  p99 {pct(f, 99):.2f}  max {max(f):.1f}")
    print(f"  fps       average {1000.0 / statistics.mean(f):.1f}  1% low {one_percent_low(f):.1f}")
    print(f"  hitches   {h} over {hitch:g} ms ({h / max(secs / 60.0, 1e-9):.1f} per minute)")
    print(f"  game ms   median {pct(g, 50):.2f}  p95 {pct(g, 95):.2f}   render ms median {pct(rt, 50):.2f}  p95 {pct(rt, 95):.2f}")
    print(f"  gpu ms    median {pct(gpu, 50):.2f}  p95 {pct(gpu, 95):.2f}  p99 {pct(gpu, 99):.2f}")
    print(f"  memory    peak resident {max(r['rss_mb'] for r in rows):.0f} MB")
    med = pct(f, 50)
    even = sum(1 for x in f if abs(x - med) <= 1.0) / len(f) * 100.0
    print(f"  pacing    {even:.1f} % of frames within 1 ms of the median")
    if "res_pct" in rows[0]:
        rp = [r["res_pct"] for r in rows]
        print(f"  render    scale median {pct(rp, 50):.0f} %  lowest {min(rp):.0f} %")


def main():
    args = sys.argv[1:]
    warmup, hitch = 5.0, 25.0
    if "--warmup" in args:
        i = args.index("--warmup")
        warmup = float(args[i + 1])
        del args[i:i + 2]
    if "--hitch" in args:
        i = args.index("--hitch")
        hitch = float(args[i + 1])
        del args[i:i + 2]
    rows = []
    with open(args[0]) as fh:
        for r in csv.DictReader(fh):
            for k in ("match_s", "frame_ms", "game_ms", "render_ms", "gpu_ms", "rss_mb", "res_pct"):
                if k in r:
                    r[k] = float(r[k])
            rows.append(r)
    warm = [r for r in rows if r["match_s"] >= warmup]
    cold = [r for r in rows if r["match_s"] < warmup]
    report(f"settled play (after the first {warmup:g} s of each match)", warm, hitch)
    report(f"match starts (first {warmup:g} s)", cold, hitch)
    scenes = sorted({r["scene"] for r in warm})
    for s in scenes:
        report("  " + s, [r for r in warm if r["scene"] == s], hitch)


if __name__ == "__main__":
    main()
