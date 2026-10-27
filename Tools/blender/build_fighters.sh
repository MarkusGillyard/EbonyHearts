#!/usr/bin/env bash
# Build and render fighters from Tools/blender/fighters.json into SourceArt/Fighters/<id>/.
#   Tools/blender/build_fighters.sh            # the vertical slice: ashka gorm
#   Tools/blender/build_fighters.sh gorm       # one fighter
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT="$(cd "$HERE/../.." && pwd)"
cd "$PROJECT"
ids=("$@")
[ ${#ids[@]} -eq 0 ] && ids=(ashka gorm)
for id in "${ids[@]}"; do
  out="$PROJECT/SourceArt/Fighters/$id"
  mkdir -p "$out"
  echo "=== $id"
  "$HERE/run_blender.sh" "$HERE/build_fighter.py" -- "$id" 2>&1 | tee "$out/${id}_build.log" \
    | grep -E 'SUMMARY|TIME|Error|Traceback' || true
  "$HERE/run_blender.sh" "$HERE/render_fighter.py" -- "$id" 2>&1 | tee "$out/${id}_render.log" \
    | grep -E 'imported|done|TIME|Error|Traceback' || true
done
