#!/usr/bin/env bash
# Cross-compile OpenXRay for one Android ABI (deps must be built first).
# Usage: [STATIC=1] scripts/build-engine-android.sh <arm64-v8a|x86_64> [configure|build]
# STATIC=1 links the whole engine into libmain.so (the layout iOS uses).
set -euo pipefail
source "$(dirname "$0")/env.sh"

ABI="${1:?usage: $0 <abi> [configure|build]}"
STEP="${2:-build}"
PREFIX="$WS/prefix/$ABI"
if [[ "${STATIC:-0}" == 1 ]]; then
    BUILD="$WS/build/engine-static/$ABI"
    SHARED=OFF
else
    BUILD="$WS/build/engine/$ABI"
    SHARED=ON
fi

if [[ "$STEP" == configure || ! -f "$BUILD/build.ninja" ]]; then
    cmake -S "$ENGINE_DIR" -B "$BUILD" $(android_cmake_args "$ABI" "$PREFIX") \
        -DCMAKE_MODULE_PATH="$ENGINE_DIR/cmake" \
        -DBUILD_SHARED_LIBS=$SHARED \
        -DXRAY_USE_LUAJIT=ON
fi
[[ "$STEP" == configure ]] && exit 0

cmake --build "$BUILD" -j "$JOBS" -- -k 0
