#!/usr/bin/env bash
# Build and render the two MPFB evaluation bodies into SourceArt/Bodies/.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT="$(cd "$HERE/../.." && pwd)"
OUT="$PROJECT/SourceArt/Bodies"
mkdir -p "$OUT"
cd "$PROJECT"

build() {  # name, args...
  local name="$1"; shift
  echo "=== $name"
  "$HERE/run_blender.sh" "$HERE/make_body.py" -- --out "$OUT/$name" "$@" 2>&1 \
    | tee "$OUT/${name}_build.log" | grep -E 'SUMMARY|TIME|Error|Traceback' || true
  "$HERE/run_blender.sh" "$HERE/render_body.py" -- --fbx "$OUT/$name.fbx" --out "$OUT/$name" 2>&1 \
    | tee "$OUT/${name}_render.log" | grep -E 'total|TIME|Error|Traceback' || true
}

# 1.74 m athletic woman
build woman_athletic_174 --gender 0.0 --height-m 1.74 --age 0.5 --muscle 0.85 --weight 0.5 \
  --proportions 0.8 --skin young_caucasian_female --eye-material bluegreen --topology base

# 2.12 m very large, heavy, muscular bald man (no hair asset)
build man_heavy_212 --gender 1.0 --height-m 2.12 --age 0.6 --muscle 1.0 --weight 1.0 \
  --proportions 0.6 --skin young_african_male --eye-material brown --topology base --hair none
