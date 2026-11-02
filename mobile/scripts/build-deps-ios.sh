#!/usr/bin/env bash
# Cross-compile the engine's third-party dependencies for iOS (arm64, static). Runs on macOS.
# Usage: scripts/build-deps-ios.sh
set -euo pipefail
source "$(dirname "$0")/env.sh"

SRC="$WS/deps-src"
PREFIX="$WS/prefix/ios-arm64"
BUILD="$WS/build/deps/ios-arm64"
mkdir -p "$PREFIX" "$BUILD"

launcher=()
if command -v ccache > /dev/null; then
    launcher=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
fi

build() {
    local name="$1" src="$2"; shift 2
    echo "=== $name (iOS)"
    cmake -S "$src" -B "$BUILD/$name" $(ios_cmake_args "$PREFIX") "${launcher[@]}" \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 "$@" > "$BUILD/$name.configure.log" 2>&1 \
        || { tail -60 "$BUILD/$name.configure.log"; exit 1; }
    cmake --build "$BUILD/$name" -j "$JOBS" > "$BUILD/$name.build.log" 2>&1 \
        || { grep -m20 -B2 -A8 "error" "$BUILD/$name.build.log" || tail -60 "$BUILD/$name.build.log"; exit 1; }
    cmake --install "$BUILD/$name" > /dev/null
}

# Everything is static: an iOS app is one executable. SDL2main provides main() for UIKit.
# HIDAPI is off: it reaches for CoreBluetooth, which needs a usage string and isn't needed
# for MFi/Xbox/PlayStation pads (those go through GameController.framework).
build SDL2 "$SRC/SDL2" -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL2MAIN=ON -DSDL_TEST=OFF -DSDL_HIDAPI=OFF

# OpenAL Soft with the CoreAudio backend. Apple's own OpenAL.framework is deprecated and
# lacks the EFX extensions the engine uses.
build openal-soft "$SRC/openal-soft" -DLIBTYPE=STATIC -DALSOFT_REQUIRE_COREAUDIO=ON \
    -DALSOFT_UTILS=OFF -DALSOFT_EXAMPLES=OFF -DALSOFT_TESTS=OFF -DALSOFT_INSTALL_CONFIG=OFF \
    -DALSOFT_INSTALL_HRTF_DATA=OFF -DALSOFT_INSTALL_AMBDEC_PRESETS=OFF

build ogg "$SRC/ogg" -DBUILD_SHARED_LIBS=OFF -DINSTALL_DOCS=OFF -DBUILD_TESTING=OFF
build vorbis "$SRC/vorbis" -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF
build theora "$MOBILE_DIR/cmake/theora" -DTHEORA_SOURCE_DIR="$SRC/theora"
build lzo "$SRC/lzo-2.10" -DENABLE_STATIC=ON -DENABLE_SHARED=OFF

# Only used for screenshots, so the SIMD paths aren't worth the extra toolchain surface.
build libjpeg-turbo "$SRC/libjpeg-turbo" -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_TURBOJPEG=OFF \
    -DWITH_TOOLS=OFF -DWITH_TESTS=OFF -DWITH_SIMD=OFF

echo "iOS deps installed to $PREFIX"
