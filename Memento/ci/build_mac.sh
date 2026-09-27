#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Build de Memento (instrument AU) + fabrication d'un .dmg à glisser.
# Appelé par GitHub Actions (runner macOS). Compile en universel (arm64+x86_64).
# ---------------------------------------------------------------------------
set -euo pipefail

HERE="$(cd "$(dirname "$0")/.." && pwd)"   # dossier Memento/
cd "$HERE"

VERSION="$(sed -n 's/^project(Memento VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)"
[ -n "$VERSION" ] || VERSION="0.0.0"
echo "==> Memento version $VERSION"

echo "==> Configuration CMake"
cmake -B build -DCMAKE_BUILD_TYPE=Release

echo "==> Compilation (AU)"
cmake --build build --config Release --target Memento_All -j 3

# Localise le bundle .component produit par JUCE.
COMPONENT="$(find build -type d -name 'Memento.component' -path '*/AU/*' | head -n 1)"
[ -n "$COMPONENT" ] || COMPONENT="$(find build -type d -name 'Memento.component' | head -n 1)"
if [ -z "$COMPONENT" ]; then echo "ERREUR : Memento.component introuvable"; find build -name '*.component' -maxdepth 6; exit 1; fi
echo "==> Composant : $COMPONENT"

echo "==> Fabrication du .dmg (glisser-déposer vers Components)"
STAGE="build/dmgroot"
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp -R "$COMPONENT" "$STAGE/"
# raccourci pratique vers le dossier d'installation des AU
ln -s "/Library/Audio/Plug-Ins/Components" "$STAGE/Components (glisser ici)"

DMG="build/Memento-${VERSION}.dmg"
rm -f "$DMG"
hdiutil create -volname "Memento ${VERSION}" \
  -srcfolder "$STAGE" -ov -format UDZO "$DMG"

echo "==> OK : $DMG"
ls -la build/*.dmg
