#!/usr/bin/env bash
# Launches the game with the given game arguments, drives it with an xdrive script through real
# X11 input, then closes it. Usage:
#   tools/playtest/play.sh <script.txt> <out_dir> [game args...]
# Example: tools/playtest/play.sh tools/playtest/scripts/slice.txt /tmp/shots --match=ashka,ashka,versus
# GAME=path/to/exported/binary runs an exported build instead of the project.
set -uo pipefail
cd "$(dirname "$0")/../.."
SCRIPT="$1"; OUT="$2"; shift 2
mkdir -p "$OUT"
TITLE="Slopbat Playtest $$"
if [ -n "${GAME:-}" ]; then
	"$GAME" -- --profile=playtest "--window-title=$TITLE" "$@" > "$OUT/game.log" 2>&1 &
else
	"${GODOT:-godot}" --path . -- --profile=playtest "--window-title=$TITLE" "$@" > "$OUT/game.log" 2>&1 &
fi
PID=$!
for i in $(seq 1 80); do
	if xwininfo -root -tree 2>/dev/null | grep -q "$TITLE"; then break; fi
	sleep 0.25
done
sleep 1.0
python3 tools/playtest/xdrive.py --window "$TITLE" --out "$OUT" "$SCRIPT"
status=$?
kill "$PID" 2>/dev/null
wait "$PID" 2>/dev/null
exit $status
