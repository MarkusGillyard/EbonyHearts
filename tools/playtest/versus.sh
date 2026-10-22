#!/usr/bin/env bash
# Versus with two inputs at once: player 1 on the keyboard (an xdrive script) and player 2 on a
# virtual Xbox 360 pad (vpad.py, a simulated controller, not physical hardware). Usage:
#   tools/playtest/versus.sh <out_dir> [keys script] [pad script]
set -uo pipefail
cd "$(dirname "$0")/../.."
OUT="$1"
KEYS="${2:-tools/playtest/scripts/versus_keys.txt}"
PAD="${3:-tools/playtest/scripts/versus_pad.txt}"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
rm -f "$OUT"/*.png
sed "s|OUT/|$OUT/|g" "$PAD" > "$OUT/pad_script.txt"
# The pad exists before the game starts, so it is Gamepad 1.
python3 tools/playtest/vpad.py "$OUT/pad_script.txt" > "$OUT/pad.log" 2>&1 &
VPAD=$!
sleep 1.0
TITLE="Slopbat Versus $$"
"${GODOT:-godot}" --path . -- --profile=playtest "--window-title=$TITLE" "--trace=$OUT/trace.jsonl" > "$OUT/game.log" 2>&1 &
PID=$!
for i in $(seq 1 80); do
	if xwininfo -root -tree 2>/dev/null | grep -q "$TITLE"; then break; fi
	sleep 0.25
done
sleep 1.0
python3 tools/playtest/xdrive.py --window "$TITLE" --out "$OUT" "$KEYS"
status=$?
wait "$VPAD"
kill "$PID" 2>/dev/null
wait "$PID" 2>/dev/null
exit $status
