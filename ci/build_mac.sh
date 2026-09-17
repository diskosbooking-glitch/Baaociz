#!/usr/bin/env bash
#
#  Skygrin - build macOS (AU + VST3) + installateur .pkg
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

STAGE="build/stage"
OUT="build/out"
rm -rf "$STAGE" "$OUT"
mkdir -p "$STAGE/Components" "$STAGE/VST3" "$OUT"

cp -R "$AU"   "$STAGE/Components/"
cp -R "$VST3" "$STAGE/VST3/"

pkgbuild --identifier com.aociz.skygrin.pkg \
         --version "${VERSION}" \
         --root "$STAGE" \
         --install-location /Library/Audio/Plug-Ins \
         "$OUT/Skygrin-${VERSION}.pkg"

echo ">>> OK : $OUT/Skygrin-${VERSION}.pkg"
