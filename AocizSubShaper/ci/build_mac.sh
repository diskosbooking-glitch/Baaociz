#!/bin/bash
# Compilation Mac (AU + VST3) -> .dmg (glisser-déposer + installateur .pkg inclus)
# Appelé par GitHub Actions (.github/workflows/build.yml)
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
AU="$ART/AU/${NAME}.component"
VST3="$ART/VST3/${NAME}.vst3"
ls -d "$AU" "$VST3"

# Signature ad-hoc : indispensable pour que macOS charge le plugin
codesign --force --deep --sign - "$AU"
codesign --force --deep --sign - "$VST3"

# ---------------------------------------------------------------- auval
mkdir -p ~/Library/Audio/Plug-Ins/Components
rm -rf ~/Library/Audio/Plug-Ins/Components/${NAME}.component
cp -R "$AU" ~/Library/Audio/Plug-Ins/Components/
killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 3
if auval -v aufx SbSh Aocz > build/auval.log 2>&1; then
    echo "==> auval : VALIDATION REUSSIE"
else
    echo "==> auval : ECHEC (voir le journal ci-dessous)"
    cat build/auval.log
fi
tail -n 20 build/auval.log

rm -rf out
mkdir -p out

# ---------------------------------------------------------------- .pkg
PAYLOAD="build/payload"
rm -rf "$PAYLOAD"
mkdir -p "$PAYLOAD/Library/Audio/Plug-Ins/Components" "$PAYLOAD/Library/Audio/Plug-Ins/VST3"
cp -R "$AU"   "$PAYLOAD/Library/Audio/Plug-Ins/Components/"
cp -R "$VST3" "$PAYLOAD/Library/Audio/Plug-Ins/VST3/"

# Bundles non relocalisables : toujours installés au même endroit
pkgbuild --analyze --root "$PAYLOAD" build/components.plist
COUNT=$(/usr/libexec/PlistBuddy -c "Print" build/components.plist | grep -c "BundleIsRelocatable" || true)
for ((i=0; i<COUNT; i++)); do
    /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" build/components.plist
done

chmod +x ci/pkg-scripts/preinstall ci/pkg-scripts/postinstall
PKG="out/${NAME}-${VERSION}.pkg"
pkgbuild --root "$PAYLOAD" \
         --component-plist build/components.plist \
         --scripts ci/pkg-scripts \
         --identifier com.aociz.subshaper.pkg \
         --version "$VERSION" \
         --install-location / \
         "$PKG"

# ---------------------------------------------------------------- .dmg
DMGROOT="build/dmgroot"
rm -rf "$DMGROOT"
mkdir -p "$DMGROOT"
cp "$PKG" "$DMGROOT/Installer Subshaper ${VERSION}.pkg"
cp -R "$AU"   "$DMGROOT/"
cp -R "$VST3" "$DMGROOT/"
ln -s "/Library/Audio/Plug-Ins/Components" "$DMGROOT/Components (glisser le .component ici)"
ln -s "/Library/Audio/Plug-Ins/VST3"       "$DMGROOT/VST3 (glisser le .vst3 ici)"
cat > "$DMGROOT/LISEZ-MOI.txt" <<EOF
Subshaper ${VERSION} - Aociz

Installation la plus simple :
  double-clic sur « Installer Subshaper ${VERSION}.pkg » (remplace les anciennes versions).

Ou à la main :
  glisser Subshaper.component sur le raccourci « Components »,
  et Subshaper.vst3 sur le raccourci « VST3 ».

Ensuite : Ableton -> Réglages -> Plug-ins -> Rescan
          FL Studio -> Options -> Manage plugins -> Find more plugins

Sidechain (PUMP en mode KICK) :
  Ableton : déplier le plugin (petit triangle) -> Sidechain -> Audio From : la piste du kick.
  FL Studio : router le kick vers la piste du plugin (sidechain), puis
  dans le wrapper -> Processing -> choisir l'entrée sidechain.
EOF

DMG="out/${NAME}-${VERSION}.dmg"
for attempt in 1 2 3 4 5; do
    if hdiutil create -volname "${NAME} ${VERSION}" -srcfolder "$DMGROOT" -ov -format UDZO "$DMG"; then
        break
    fi
    if [ "$attempt" -eq 5 ]; then echo "==> hdiutil : échec définitif"; exit 1; fi
    echo "==> hdiutil a échoué (essai $attempt), nouvel essai dans 10 s"
    sleep 10
done

echo "==> Prêt :"
ls -la out
