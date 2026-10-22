#!/usr/bin/env bash
# Launches the game and lets autoplayer.py play it as player 1 through real X11 input for a
# while, with screenshots, then closes it. Usage:
#   tools/playtest/autoplay.sh <out_dir> <seconds> [autoplayer args --] [game args...]
# Example (a whole arcade run from the title screen):
#   tools/playtest/autoplay.sh /tmp/arcade 900
# GAME=path/to/exported/binary runs an exported build instead of the project. PRELUDE=script.txt
# first plays an xdrive script (to pick a mode, fighter or tower) before the autoplayer takes over.
set -uo pipefail
cd "$(dirname "$0")/../.."
OUT="$1"; SECS="$2"; shift 2
AP=()
if [[ " $* " == *" -- "* ]]; then
	while [ "$1" != "--" ]; do AP+=("$1"); shift; done
	shift
fi
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
TRACE="$OUT/trace.jsonl"
rm -f "$TRACE"
TITLE="Slopbat Autoplay $$"
if [ -n "${GAME:-}" ]; then
	"$GAME" -- --profile=playtest "--window-title=$TITLE" "--trace=$TRACE" --trace-live "$@" > "$OUT/game.log" 2>&1 &
else
	"${GODOT:-godot}" --path . -- --profile=playtest "--window-title=$TITLE" "--trace=$TRACE" --trace-live "$@" > "$OUT/game.log" 2>&1 &
fi
PID=$!
for i in $(seq 1 80); do
	if xwininfo -root -tree 2>/dev/null | grep -q "$TITLE"; then break; fi
	sleep 0.25
done
sleep 1.0
# The game's memory once a minute, for the record of long sessions.
( while kill -0 "$PID" 2>/dev/null; do echo "$(date +%T) rss_kb=$(ps -o rss= -p "$PID")"; sleep 60; done ) > "$OUT/resources.txt" 2>/dev/null &
SAMPLER=$!
if [ -n "${PRELUDE:-}" ]; then
	python3 tools/playtest/xdrive.py --window "$TITLE" --out "$OUT" "$PRELUDE"
fi
python3 tools/playtest/autoplayer.py --window "$TITLE" --trace "$TRACE" --seconds "$SECS" --shots "$OUT" --every 15 "${AP[@]}"
status=$?
echo "$(date +%T) rss_kb=$(ps -o rss= -p "$PID") at the end" >> "$OUT/resources.txt"
kill "$SAMPLER" "$PID" 2>/dev/null
wait "$PID" 2>/dev/null
exit $status
