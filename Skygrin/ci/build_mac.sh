#!/usr/bin/env bash
#
#  Skygrin - build macOS (AU + VST3) -> .dmg a glisser-deposer + .pkg de secours
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

VERSION="$(grep -m1 -Eo 'VERSION[[:space:]]+[0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt | awk '{print $2}')"
echo ">>> Skygrin version ${VERSION}"

cmake -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13

cmake --build build --config Release --parallel 3

ART="build/Skygrin_artefacts/Release"
AU="${ART}/AU/Skygrin.component"
VST3="${ART}/VST3/Skygrin.vst3"

ls -d "$AU" "$VST3"

# signature ad-hoc : indispensable pour que macOS charge le plugin
codesign --force --deep --sign - "$AU"
codesign --force --deep --sign - "$VST3"

OUT="build/out"
rm -rf "$OUT"
mkdir -p "$OUT"

# ---- .dmg : on l'ouvre et on glisse chaque plugin sur le raccourci de son dossier
DMGROOT="build/dmgroot"
rm -rf "$DMGROOT"
mkdir -p "$DMGROOT"
cp -R "$AU"   "$DMGROOT/"
cp -R "$VST3" "$DMGROOT/"
ln -s "/Library/Audio/Plug-Ins/Components" "$DMGROOT/Components (glisser ici)"
ln -s "/Library/Audio/Plug-Ins/VST3"       "$DMGROOT/VST3 (glisser ici)"

DMG="$OUT/Skygrin-${VERSION}.dmg"
for attempt in 1 2 3 4 5; do
    if hdiutil create -volname "Skygrin ${VERSION}" -srcfolder "$DMGROOT" -ov -format UDZO "$DMG"; then
        break
    fi
    if [ "$attempt" -eq 5 ]; then echo ">>> hdiutil : echec definitif"; exit 1; fi
    echo ">>> hdiutil a echoue (essai ${attempt}), nouvel essai dans 10 s"
    sleep 10
done

# ---- .pkg de secours : installation AU + VST3 en une commande Terminal
STAGE="build/stage"
rm -rf "$STAGE"
mkdir -p "$STAGE/Components" "$STAGE/VST3"
cp -R "$AU"   "$STAGE/Components/"
cp -R "$VST3" "$STAGE/VST3/"

pkgbuild --identifier com.aociz.skygrin.pkg \
         --version "${VERSION}" \
         --root "$STAGE" \
         --install-location /Library/Audio/Plug-Ins \
         "$OUT/Skygrin-${VERSION}.pkg"

echo ">>> OK :"
ls -la "$OUT"
