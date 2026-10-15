#!/bin/sh
# Builds NullAtomo.vst3 in this folder and installs it where Reason scans.
set -eu
cd "$(dirname "$0")"

if command -v cmake >/dev/null 2>&1; then
    CMAKE=cmake
elif [ -x /opt/homebrew/bin/cmake ]; then
    CMAKE=/opt/homebrew/bin/cmake
elif [ -x /tmp/cmake-mac/cmake-3.31.8-macos-universal/CMake.app/Contents/bin/cmake ]; then
    CMAKE=/tmp/cmake-mac/cmake-3.31.8-macos-universal/CMake.app/Contents/bin/cmake
elif [ -x .tools/cmake/CMake.app/Contents/bin/cmake ]; then
    CMAKE=.tools/cmake/CMake.app/Contents/bin/cmake
else
    echo "cmake not found." >&2
    exit 1
fi

"$CMAKE" -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
"$CMAKE" --build build --config Release --parallel

ART=$(find build -type d -name "NullAtomo.vst3" | head -1)
if [ -z "$ART" ]; then
    echo "VST3 bundle was not produced." >&2
    exit 1
fi

rm -rf "NULL GLITCH.vst3" "NullAtomo.vst3"
ditto "$ART" "NullAtomo.vst3"
codesign --force --deep --sign - "NullAtomo.vst3"

DEST="$HOME/Library/Audio/Plug-Ins/VST/NullAtomo.vst3"
rm -rf "$HOME/Library/Audio/Plug-Ins/VST/NULL GLITCH.vst3" "$DEST"
ditto "NullAtomo.vst3" "$DEST"
codesign --force --deep --sign - "$DEST"
echo "Built $(pwd)/NullAtomo.vst3"
echo "Installed $DEST"
