#!/usr/bin/env bash
# Cross-compile OpenXRay for iOS as one static app bundle (deps must be built first). Runs on macOS.
# Usage: [IOS_BUILD_NUMBER=n] scripts/build-engine-ios.sh [configure|build]
# The full build log goes to $WS/logs/engine-ios.log; errors are summarized at the end.
set -euo pipefail
source "$(dirname "$0")/env.sh"

STEP="${1:-build}"
PREFIX="$WS/prefix/ios-arm64"
BUILD="$WS/build/engine-ios"
LOG="$WS/logs/engine-ios.log"
mkdir -p "$WS/logs"

if [[ "$STEP" == configure || ! -f "$BUILD/build.ninja" ]]; then
    # FindOpenAL would pick Apple's deprecated OpenAL.framework, so point it at OpenAL Soft.
    # LTO is off: it roughly doubles the link time on CI for little gain while porting.
    cmake -S "$ENGINE_DIR" -B "$BUILD" $(ios_cmake_args "$PREFIX") \
        -DCMAKE_MODULE_PATH="$ENGINE_DIR/cmake" \
        -DBUILD_SHARED_LIBS=OFF \
        -DXRAY_USE_LUAJIT=ON \
        -DXRAY_USE_LTO=OFF \
        -DOPENAL_INCLUDE_DIR="$PREFIX/include/AL" \
        -DOPENAL_LIBRARY="$PREFIX/lib/libopenal.a" \
        -DXRAY_IOS_BUILD_NUMBER="${IOS_BUILD_NUMBER:-1}"
fi
[[ "$STEP" == configure ]] && exit 0

# -k 0: one CI run should surface every compile error, not just the first one.
status=0
cmake --build "$BUILD" -j "$JOBS" -- -k 0 > "$LOG" 2>&1 || status=$?
if (( status != 0 )); then
    echo "=== build failed, errors:"
    grep -E "error:|Error |undefined symbol|ld: " "$LOG" | sort -u | head -150
    echo "=== last lines:"
    tail -30 "$LOG"
    exit $status
fi
echo "engine built: $(find "$ENGINE_DIR/bin/ios-arm64" -maxdepth 2 -name '*.app' | head -1)"
