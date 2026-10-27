#!/usr/bin/env bash
# Plays one scripted menu flow (Tools/playtest/flows/<name>.txt: a -SlopUi action list, a line of
# extra game flags, and a time limit) in a separate test profile, with a screenshot every few
# seconds, and prints the screens it passed through.
#
#   Tools/playtest/run_flow.sh <name> [game binary]     (default: the editor in -game mode)
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
# shellcheck source=../ue.sh
source "$ROOT/Tools/ue.sh"
FLOW="$HERE/flows/$1.txt"
UI="$(grep -v '^#' "$FLOW" | sed -n 1p)"
EXTRA="$(grep -v '^#' "$FLOW" | sed -n 2p)"
LIMIT="$(grep -v '^#' "$FLOW" | sed -n 3p)"
SHOTS="$ROOT/Saved/Screenshots/LinuxEditor"
rm -f "$SHOTS"/flow_"$1"_*.png
args=(-SlopUi="$UI" -SlopShotEvery=2 -SlopShotPrefix="flow_$1" -SlopQuitAfter="$LIMIT" -profile=autotest $EXTRA)
SAVE="$HOME/.local/share/MortalSlopbat/UE5"
START="$(mktemp)"
if [ $# -ge 2 ]; then
	LOG="$(dirname "$2")/MortalSlopbat/Saved/Logs/MortalSlopbat.log"
	timeout -k 5 $((LIMIT + 120)) systemd-run --user --scope -p MemoryMax=9G --quiet "$2" -windowed -resx=1920 -resy=1080 $(offscreen_flag) "${args[@]}" > /dev/null 2>&1
else
	LOG="$ROOT/Saved/Logs/MortalSlopbat.log"
	timeout -k 5 $((LIMIT + 120)) bash -c "source '$ROOT/Tools/ue.sh'; run_game $(printf '%q ' "${args[@]}")" > /dev/null 2>&1
fi
pkill -9 -x UnrealEditor 2>/dev/null
# The run's frame stats go with the project's other records, and the test profile is cleared, so
# nothing is left beside the player's own files.
mkdir -p "$ROOT/Saved/Perf/flow-$1"
find "$SAVE" -maxdepth 1 -name 'framestats-*' -newer "$START" -exec mv -t "$ROOT/Saved/Perf/flow-$1" {} +
rm -f "$SAVE"/settings_autotest.json* "$SAVE"/progress_autotest.json* "$START"
grep -E "LogSlop: (Screen|Overlay|Match|Result|Ui script)|Assertion failed|Fatal error|Ensure condition" "$LOG" | sed -E 's/^\[[0-9.]+-([0-9.]+):[0-9]+\]\[ *[0-9]+\]/\1 /'
