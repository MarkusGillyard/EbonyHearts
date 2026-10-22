#!/usr/bin/env bash
# Exports the release build and packages it as dist/MortalSlopbat-<version>-linux-x86_64.tar.gz
# with its .sha256. Requires the Godot 4.7.2 editor on PATH (or GODOT=/path/to/godot) and the
# 4.7.2 export templates.
set -euo pipefail
cd "$(dirname "$0")/.."
GODOT="${GODOT:-godot}"
VERSION="$(sed -n 's/^config\/version="\(.*\)"/\1/p' project.godot)"
NAME="MortalSlopbat-${VERSION}-linux-x86_64"
OUT="build/${NAME}"
rm -rf "$OUT"
mkdir -p "$OUT" dist
"$GODOT" --headless --import >/dev/null 2>&1 || true
"$GODOT" --headless --export-release "Linux" "$OUT/MortalSlopbat.x86_64"
cp tools/dist/MortalSlopbat.sh tools/dist/install-desktop-entry.sh "$OUT/"
cp assets/icon.png "$OUT/icon.png"
cp LICENSE THIRD_PARTY_NOTICES.md "$OUT/"
cp docs/PLAYING.txt "$OUT/README.txt"
cp -r licenses "$OUT/"
chmod +x "$OUT/MortalSlopbat.x86_64" "$OUT/MortalSlopbat.sh" "$OUT/install-desktop-entry.sh"
tar -C build -czf "dist/${NAME}.tar.gz" "$NAME"
(cd dist && sha256sum "${NAME}.tar.gz" > "${NAME}.tar.gz.sha256")
echo "Built dist/${NAME}.tar.gz"
cat "dist/${NAME}.tar.gz.sha256"
