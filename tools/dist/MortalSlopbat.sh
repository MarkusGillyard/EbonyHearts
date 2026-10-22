#!/usr/bin/env bash
# Launches Mortal Slopbat from the folder this script is in, wherever that is. Extra arguments go
# to the game, e.g. ./MortalSlopbat.sh --rendering-driver opengl3
cd "$(dirname "$(readlink -f "$0")")" || exit 1
exec ./MortalSlopbat.x86_64 "$@"
