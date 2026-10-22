#!/usr/bin/env bash
# Adds Mortal Slopbat to your desktop's application menu (for this user only; no root needed).
# Run it from the extracted game folder. Remove with: rm ~/.local/share/applications/mortal-slopbat.desktop
set -euo pipefail
here="$(dirname "$(readlink -f "$0")")"
apps="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
mkdir -p "$apps"
cat > "$apps/mortal-slopbat.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Mortal Slopbat
Comment=An arcade fighting game
Exec="$here/MortalSlopbat.sh"
Icon=$here/icon.png
Path=$here
Terminal=false
Categories=Game;ActionGame;
DESKTOP
chmod +x "$apps/mortal-slopbat.desktop"
echo "Added Mortal Slopbat to the application menu ($apps/mortal-slopbat.desktop)."
