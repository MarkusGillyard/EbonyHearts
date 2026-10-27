#!/usr/bin/env bash
# Run Blender headless against the isolated MPFB test config in .blender_user/,
# inside a memory-capped systemd scope, and report wall time + peak RSS.
#
#   Tools/blender/run_blender.sh <script.py> [-- script args...]
#
# Never touches ~/.config/blender: every Blender user path is redirected below.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
U="$HERE/.blender_user"
BLENDER="${BLENDER:-$HOME/.local/bin/blender}"

# BLENDER_USER_RESOURCES moves bpy.utils.resource_path('USER'), which MPFB's LogService
# writes logs/config under; without it MPFB writes into ~/.config/blender/<ver>/mpfb.
export BLENDER_USER_RESOURCES="$U"
export BLENDER_USER_CONFIG="$U/config"
export BLENDER_USER_SCRIPTS="$U/scripts"
export BLENDER_USER_EXTENSIONS="$U/extensions"
export BLENDER_USER_DATAFILES="$U/datafiles"
mkdir -p "$BLENDER_USER_CONFIG" "$BLENDER_USER_SCRIPTS" "$BLENDER_USER_EXTENSIONS" "$BLENDER_USER_DATAFILES"

script="$1"; shift
exec systemd-run --user --scope -p MemoryMax=5G -p MemorySwapMax=512M --quiet \
  /usr/bin/time -f 'TIME wall=%es user=%Us sys=%Ss maxrss=%MKB exit=%x' \
  "$BLENDER" -b --factory-startup --python-exit-code 1 -P "$script" "$@"
