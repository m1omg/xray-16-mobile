#!/usr/bin/env bash
# Cross-compile the engine's third-party dependencies for one Android ABI.
# Usage: scripts/build-deps-android.sh <arm64-v8a|x86_64>
set -euo pipefail
source "$(dirname "$0")/env.sh"

ABI="${1:?usage: $0 <abi>}"
SRC="$WS/deps-src"
PREFIX="$WS/prefix/$ABI"
BUILD="$WS/build/deps/$ABI"
mkdir -p "$PREFIX" "$BUILD"

build() {
    local name="$1" src="$2"; shift 2
    echo "=== $name ($ABI)"
    cmake -S "$src" -B "$BUILD/$name" $(android_cmake_args "$ABI" "$PREFIX") "$@" > "$BUILD/$name.configure.log" 2>&1 \
        || { tail -40 "$BUILD/$name.configure.log"; exit 1; }
    cmake --build "$BUILD/$name" -j "$JOBS" > "$BUILD/$name.build.log" 2>&1 \
        || { grep -m20 -B2 -A8 "error" "$BUILD/$name.build.log" || tail -40 "$BUILD/$name.build.log"; exit 1; }
    cmake --install "$BUILD/$name" > /dev/null
}

# SDL2 must be shared: SDLActivity System.loadLibrary()s libSDL2.so before libmain.so.
build SDL2 "$SRC/SDL2" -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF

# OpenAL Soft: shared (LGPL), OpenSL ES / AAudio backends are picked up from the NDK.
build openal-soft "$SRC/openal-soft" -DLIBTYPE=SHARED -DALSOFT_UTILS=OFF -DALSOFT_EXAMPLES=OFF \
    -DALSOFT_TESTS=OFF -DALSOFT_INSTALL_CONFIG=OFF -DALSOFT_INSTALL_HRTF_DATA=OFF -DALSOFT_INSTALL_AMBDEC_PRESETS=OFF

build ogg "$SRC/ogg" -DBUILD_SHARED_LIBS=OFF -DINSTALL_DOCS=OFF -DBUILD_TESTING=OFF
build vorbis "$SRC/vorbis" -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF
build theora "$MOBILE_DIR/cmake/theora" -DTHEORA_SOURCE_DIR="$SRC/theora"
build lzo "$SRC/lzo-2.10" -DENABLE_STATIC=ON -DENABLE_SHARED=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5

# libjpeg-turbo's x86 SIMD needs NASM, which we don't ship; NEON on arm64 is plain intrinsics.
jpeg_simd=ON; [[ "$ABI" == x86* ]] && jpeg_simd=OFF
build libjpeg-turbo "$SRC/libjpeg-turbo" -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_TURBOJPEG=OFF \
    -DWITH_TOOLS=OFF -DWITH_TESTS=OFF -DWITH_SIMD=$jpeg_simd

echo "deps for $ABI installed to $PREFIX"
