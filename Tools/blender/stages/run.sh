#!/usr/bin/env bash
# Run Blender headless for the stage builders, with an isolated user config under
# .blender_user/ (never ~/.config/blender), inside a memory-capped systemd scope.
#
#   Tools/blender/stages/run.sh build_rendering_yards.py [-- --no-render | --quick]
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
U="$HERE/.blender_user"
BLENDER="${BLENDER:-$HOME/.local/bin/blender}"
export BLENDER_USER_RESOURCES="$U"
export BLENDER_USER_CONFIG="$U/config"
export BLENDER_USER_SCRIPTS="$U/scripts"
export BLENDER_USER_EXTENSIONS="$U/extensions"
export BLENDER_USER_DATAFILES="$U/datafiles"
mkdir -p "$BLENDER_USER_CONFIG" "$BLENDER_USER_SCRIPTS" "$BLENDER_USER_EXTENSIONS" "$BLENDER_USER_DATAFILES"
script="$1"; shift
case "$script" in /*) ;; *) script="$HERE/$script" ;; esac
exec systemd-run --user --scope -p MemoryMax=6G -p MemorySwapMax=512M --quiet \
  /usr/bin/time -f 'TIME wall=%es maxrss=%MKB exit=%x' \
  "$BLENDER" -b --factory-startup --disable-autoexec --python-exit-code 1 -P "$script" "$@"
