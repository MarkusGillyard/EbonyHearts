#!/usr/bin/env bash
# Launches the game and lets autoplayer.py play it as player 1 through real X11 input for a
# while, with screenshots, then closes it. Usage:
#   tools/playtest/autoplay.sh <out_dir> <seconds> [autoplayer args --] [game args...]
# Example (a whole arcade run from the title screen):
#   tools/playtest/autoplay.sh /tmp/arcade 900
# GAME=path/to/exported/binary runs an exported build instead of the project.
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
python3 tools/playtest/autoplayer.py --window "$TITLE" --trace "$TRACE" --seconds "$SECS" --shots "$OUT" --every 15 "${AP[@]}"
status=$?
# Resources at the end of the session, for the record.
ps -o rss=,etime= -p "$PID" > "$OUT/resources.txt" 2>/dev/null
kill "$PID" 2>/dev/null
wait "$PID" 2>/dev/null
exit $status
