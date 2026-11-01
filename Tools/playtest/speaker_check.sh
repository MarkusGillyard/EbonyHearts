#!/usr/bin/env bash
# Plays a real fight into a virtual output for each speaker layout and measures every channel, so
# levels, clipping and silent speakers can be compared without owning the hardware. TV speakers,
# headphones and stereo speakers are the stereo layout; soundbars are stereo, 2.1 or 5.1. It shows
# what the game sends, not how a room sounds. A small game window opens for each layout. The game opens
# the default output, so its stream is moved to the virtual one as soon as it appears: the first
# moment of a run can reach the real speakers, but the default output is never changed. The game
# asks pactl what the default output is, so a stand-in pactl on the game's PATH names the virtual
# output instead: the automatic choice of mix is exercised exactly as a player's would be.
#
#   Tools/playtest/speaker_check.sh [package dir] [seconds] [speakers setting]
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
# shellcheck source=../ue.sh
source "$ROOT/Tools/ue.sh"
PKG="${1:-$BUILDS/Shipping/Linux}"; SECS="${2:-30}"; SPEAKERS="${3:-}"
OUT="$ROOT/Saved/Speakers"; mkdir -p "$OUT"
SAVES="${XDG_DATA_HOME:-$HOME/.local/share}/MortalSlopbat/UE5"
declare -A MAPS=(
	[mono]="1 mono"
	[stereo]="2 front-left,front-right"
	[2.1]="3 front-left,front-right,lfe"
	[quad]="4 front-left,front-right,rear-left,rear-right"
	[5.1]="6 front-left,front-right,front-center,lfe,rear-left,rear-right"
	[7.1]="8 front-left,front-right,front-center,lfe,rear-left,rear-right,side-left,side-right"
)
MODS=()
cleanup() { for m in "${MODS[@]:-}"; do [ -n "$m" ] && pactl unload-module "$m" 2>/dev/null; done; rm -f "$SAVES"/settings_speakers.json* "$SAVES"/progress_speakers.json*; }
trap cleanup EXIT
for L in ${LAYOUTS:-mono stereo 2.1 quad 5.1 7.1}; do
	read -r N MAP <<< "${MAPS[$L]}"
	S="eh_check_$(echo "$L" | tr -d .)"
	M="$(pactl load-module module-null-sink sink_name="$S" rate=48000 format=float32le channels="$N" channel_map="$MAP" sink_properties=device.description="$S")"; MODS+=("$M")
	[ -n "$SPEAKERS" ] && echo "{\"version\": 2, \"speakers\": \"$SPEAKERS\"}" > "$SAVES/settings_speakers.json"
	SHIM="$OUT/shim"; mkdir -p "$SHIM"
	REAL="$(command -v pactl)"
	printf '#!/bin/sh\n"%s" "$@" | sed "s/^Default Sink: .*/Default Sink: %s/"\n' "$REAL" "$S" > "$SHIM/pactl"; chmod +x "$SHIM/pactl"
	PATH="$SHIM:$PATH" "$PKG/MortalSlopbat.sh" -windowed -resx=640 -resy=360 -profile=speakers -SlopNoPads \
		-SlopSoakMatch=ashka,gorm,rendering_yards -SlopQuitAfter=$((SECS + 12)) > /dev/null 2>&1 &
	G=$!
	ID=""
	for _ in $(seq 1 400); do
		ID="$(pactl list sink-inputs 2>/dev/null | awk '/^Sink Input #/ {id = substr($3, 2)} /application.process.binary = "MortalSlopbat/ {print id; exit}')"
		[ -n "$ID" ] && break
		python3 -c 'import time; time.sleep(0.1)'
	done
	# A stream moved in its first moments can fall back to the default output, so it is moved until it stays.
	for _ in 1 2 3 4 5 6 7 8 9; do
		[ -n "$ID" ] && pactl move-sink-input "$ID" "$S" 2>/dev/null
		python3 -c 'import time; time.sleep(1)'
	done
	if ! pactl list short sink-inputs | awk -v id="$ID" -v sink="$(pactl list short sinks | awk -v s="$S" '$2 == s {print $1}')" '$1 == id && $2 == sink {found = 1} END {exit !found}'; then
		echo "$L: the game's stream did not reach the virtual output"
	fi
	timeout "$SECS" parec -d "$S.monitor" --rate=48000 --format=float32le --channels="$N" --channel-map="$MAP" --raw > "$OUT/$L.raw" 2>/dev/null
	wait "$G"
	python3 -I "$HERE/speaker_stats.py" "$OUT/$L.raw" "$N" "$L"
	rm -f "$OUT/$L.raw"
done
