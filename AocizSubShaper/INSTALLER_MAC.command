#!/bin/bash
# Installe Aociz SubShaper (AU + VST3) — double-cliquer sur ce fichier.
cd "$(dirname "$0")"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$AU_DIR" "$VST3_DIR"

rm -rf "$AU_DIR/Aociz SubShaper.component" "$VST3_DIR/Aociz SubShaper.vst3"
cp -R "Aociz SubShaper.component" "$AU_DIR/"
cp -R "Aociz SubShaper.vst3" "$VST3_DIR/"

# Plugin non signé par Apple : on retire la quarantaine
xattr -dr com.apple.quarantine "$AU_DIR/Aociz SubShaper.component" 2>/dev/null
xattr -dr com.apple.quarantine "$VST3_DIR/Aociz SubShaper.vst3" 2>/dev/null

# Force macOS à relire la liste des Audio Units
killall -9 AudioComponentRegistrar 2>/dev/null

echo ""
echo "  Aociz SubShaper installé."
echo "  -> Relance Ableton / FL Studio puis fais un rescan des plugins."
echo ""
read -n 1 -s -r -p "  Appuie sur une touche pour fermer..."
