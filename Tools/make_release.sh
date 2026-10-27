#!/usr/bin/env bash
# Wraps a packaged build as a release archive: the game and its launcher, the player readme, the
# licence texts, a version file naming the source commit, a checksum list of every file, and the
# archive's own checksum. Run after `package_linux <config>` (Tools/ue.sh).
#
#   Tools/make_release.sh [Shipping|Development]
#
# Output: $BUILDS/release/MortalSlopbat-<version>-linux-x86_64[-Development].tar.gz (+ .sha256)
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=ue.sh
source "$ROOT/Tools/ue.sh"
CFG="${1:-Shipping}"
VER="$(sed -n 's/^ProjectVersion=//p' "$ROOT/Config/DefaultGame.ini")"
REV="$(git -C "$ROOT" rev-parse --short=10 HEAD)"
if [ -n "$(git -C "$ROOT" status --porcelain -- Source Config Content)" ]; then
	echo "warning: Source, Config or Content differ from commit $REV" >&2
fi
SRC="$BUILDS/$CFG/Linux"
NAME="MortalSlopbat-$VER-linux-x86_64"
[ "$CFG" = Shipping ] || NAME="$NAME-$CFG"
OUT="$BUILDS/release/$NAME"
[ -x "$SRC/MortalSlopbat.sh" ] || { echo "no package at $SRC (run package_linux $CFG first)" >&2; exit 1; }

rm -rf "$OUT" "$OUT.tar.gz" "$OUT.tar.gz.sha256"
mkdir -p "$OUT/Licenses"
cp -a "$SRC/." "$OUT/"
rm -f "$OUT"/Manifest_*.txt
cp "$ROOT/Docs/Player-README.txt" "$OUT/README.txt"
cp "$ROOT/LICENSE" "$OUT/LICENSE.txt"
cp "$ROOT"/Docs/Licenses/* "$OUT/Licenses/"
printf 'Mortal Slopbat %s (%s build of source commit %s, %s)\n' "$VER" "$CFG" "$REV" "$(date -u +%Y-%m-%d)" > "$OUT/VERSION.txt"
chmod +x "$OUT/MortalSlopbat.sh"
(cd "$OUT" && find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS)
tar -C "$BUILDS/release" -czf "$OUT.tar.gz" "$NAME"
(cd "$BUILDS/release" && sha256sum "$NAME.tar.gz" > "$NAME.tar.gz.sha256")
du -h "$OUT.tar.gz" | cut -f1
cat "$OUT.tar.gz.sha256"
