#!/bin/bash
# Compilation locale sur Mac (nécessite Xcode + CMake : brew install cmake)
set -e
cd "$(dirname "$0")"
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSUBSHAPER_COPY_AFTER_BUILD=ON
cmake --build build --config Release --target Subshaper_AU Subshaper_VST3 -j 8
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "OK : Subshaper compilé et copié dans ~/Library/Audio/Plug-Ins"
