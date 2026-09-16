#!/bin/bash
# Compilation Mac + création de l'installateur .pkg (appelé par GitHub Actions)
set -euo pipefail
cd "$(dirname "$0")/.."

NAME="Subshaper"
VERSION=$(sed -n 's/^project(Subshaper VERSION \([0-9.]*\)).*/\1/p' CMakeLists.txt)
echo "==> $NAME $VERSION"

cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release --target ${NAME}_AU ${NAME}_VST3 -j 4

ART="build/${NAME}_artefacts/Release"
PAYLOAD="build/payload"
rm -rf "$PAYLOAD" out
mkdir -p "$PAYLOAD/Library/Audio/Plug-Ins/Components" "$PAYLOAD/Library/Audio/Plug-Ins/VST3" out

cp -R "$ART/AU/${NAME}.component" "$PAYLOAD/Library/Audio/Plug-Ins/Components/"
cp -R "$ART/VST3/${NAME}.vst3"    "$PAYLOAD/Library/Audio/Plug-Ins/VST3/"
codesign --force --deep --sign - "$PAYLOAD/Library/Audio/Plug-Ins/Components/${NAME}.component"
codesign --force --deep --sign - "$PAYLOAD/Library/Audio/Plug-Ins/VST3/${NAME}.vst3"

# Validation Audio Unit (informative)
mkdir -p ~/Library/Audio/Plug-Ins/Components
cp -R "$PAYLOAD/Library/Audio/Plug-Ins/Components/${NAME}.component" ~/Library/Audio/Plug-Ins/Components/
killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 3
if auval -v aufx SbSh Aocz > build/auval.log 2>&1; then
    echo "==> auval : VALIDATION REUSSIE"
else
    echo "==> auval : ECHEC (voir ci-dessous)"
fi
tail -n 25 build/auval.log

# Installateur : bundles non relocalisables (installés toujours au même endroit)
pkgbuild --analyze --root "$PAYLOAD" build/components.plist
COUNT=$(/usr/libexec/PlistBuddy -c "Print" build/components.plist | grep -c "BundleIsRelocatable" || true)
for ((i=0; i<COUNT; i++)); do
    /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" build/components.plist
done

chmod +x ci/pkg-scripts/preinstall ci/pkg-scripts/postinstall
pkgbuild --root "$PAYLOAD" \
         --component-plist build/components.plist \
         --scripts ci/pkg-scripts \
         --identifier com.aociz.subshaper.pkg \
         --version "$VERSION" \
         --install-location / \
         "out/${NAME}-${VERSION}.pkg"

echo "==> Installateur prêt : out/${NAME}-${VERSION}.pkg"
