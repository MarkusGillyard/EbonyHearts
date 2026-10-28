#!/usr/bin/env bash
# An unattended packaged session for the performance and reliability record: the CPU plays
# (-SlopAutoPlay) for the given time while the game writes its frame times (-SlopFrameStats) and
# this script samples the process's resident memory and the GPU's memory every five seconds.
#
#   Tools/perf/soak.sh <package dir> <seconds> [label] [extra game flags...]
#
# SOAK_MATCH=p1,p2[,stage] plays that one match over and over instead (-SlopSoakMatch), so memory
# can be read without the changes that different fighters and stages bring.
#
# Results go to Saved/Perf/<label>-<time>/ (frame CSV, memory CSV, summary.txt, game log).
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# shellcheck source=../ue.sh
source "$ROOT/Tools/ue.sh"
PKG="$1"; SECS="$2"; LABEL="${3:-soak}"; shift 3 2>/dev/null || shift $#
OUT="$ROOT/Saved/Perf/$LABEL-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT"
SAVES="${XDG_DATA_HOME:-$HOME/.local/share}/MortalSlopbat/UE5"
BEFORE="$(ls "$SAVES"/framestats-*.csv 2>/dev/null)"
nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader > "$OUT/gpu.txt" 2>/dev/null
timeout -k 10 $((SECS + 180)) systemd-run --user --scope -p MemoryMax=10G --quiet "$PKG/MortalSlopbat.sh" -windowed -resx=1920 -resy=1080 \
	$(offscreen_flag) $([ -n "${SOAK_MATCH:-}" ] && echo "-SlopSoakMatch=$SOAK_MATCH" || echo -SlopAutoPlay=2) -SlopFrameStats -SlopQuitAfter="$SECS" -profile="$LABEL" "$@" > "$OUT/stdout.log" 2>&1 &
GAME=$!
sleep 5
echo "t_s,rss_mb,gpu_used_mb" > "$OUT/memory.csv"
T0=$(date +%s)
while true; do
	PID="$(ps -eo pid,args | awk -v p="$PKG/MortalSlopbat/Binaries/Linux/MortalSlopbat" 'index($0, p) && !/awk/ {print $1; exit}')"
	[ -n "$PID" ] || break
	RSS=$(awk '/VmRSS/ {print $2/1024}' "/proc/$PID/status" 2>/dev/null)
	GPU=$(nvidia-smi --query-compute-apps=pid,used_memory --format=csv,noheader,nounits 2>/dev/null | awk -F', ' -v p="$PID" '$1==p {print $2}')
	[ -n "$GPU" ] || GPU=$(nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits 2>/dev/null)
	echo "$(( $(date +%s) - T0 )),${RSS:-0},${GPU:-0}" >> "$OUT/memory.csv"
	sleep 5
done
wait "$GAME"; RC=$?
NEW="$(comm -13 <(echo "$BEFORE" | sort) <(ls "$SAVES"/framestats-*.csv 2>/dev/null | sort) | tail -1)"
if [ -n "$NEW" ]; then
	mv "$NEW" "$OUT/frames.csv"
	mv "${NEW%.csv}.events.txt" "$OUT/events.txt" 2>/dev/null
fi
# Development packages keep a log beside the game; Shipping ones keep none (the events file
# stands in).
cp "$PKG/MortalSlopbat/Saved/Logs/MortalSlopbat.log" "$OUT/game.log" 2>/dev/null
{
	echo "package: $PKG"; cat "$OUT/gpu.txt"
	echo "game exit status: $RC (0: quit by itself at the set time)"
	grep -c "Result:" "$OUT/events.txt" 2>/dev/null | sed 's/^/matches finished: /'
	grep -c "Quit after the set time" "$OUT/events.txt" 2>/dev/null | sed 's/^/ran to the set time: /'
	[ -f "$OUT/game.log" ] && grep -cE "Assertion failed|Fatal error|Ensure condition" "$OUT/game.log" | sed 's/^/assertions, fatal errors, ensures in the log: /'
	[ -f "$OUT/frames.csv" ] && python3 -I "$ROOT/Tools/perf/summarize.py" "$OUT/frames.csv"
	python3 -I - "$OUT/memory.csv" <<'EOF'
import csv, sys
rows = [r for r in csv.DictReader(open(sys.argv[1]))]
if rows:
    rss = [float(r["rss_mb"]) for r in rows]; gpu = [float(r["gpu_used_mb"]) for r in rows]
    n = max(1, len(rows) // 10)
    print(f"process memory: first tenth {sum(rss[:n]) / n:.0f} MB, last tenth {sum(rss[-n:]) / n:.0f} MB, peak {max(rss):.0f} MB")
    print(f"GPU memory: first tenth {sum(gpu[:n]) / n:.0f} MB, last tenth {sum(gpu[-n:]) / n:.0f} MB, peak {max(gpu):.0f} MB")
EOF
} > "$OUT/summary.txt"
cat "$OUT/summary.txt"
echo "$OUT"
