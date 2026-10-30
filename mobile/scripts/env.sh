# Shared environment for the Android build scripts. Source it, don't run it.
# MOBILE_DIR: this port's tooling (mobile/ in the engine repo)
# ENGINE_DIR: the OpenXRay checkout it lives in
# WS: work dir for toolchains, dependency sources, build trees and logs; kept outside the
#     repo. Defaults to the directory containing the engine checkout.
MOBILE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE_DIR="$(cd "$MOBILE_DIR/.." && pwd)"
WS="${XRAY_MOBILE_WORK:-$(cd "$ENGINE_DIR/.." && pwd)}"
export MOBILE_DIR ENGINE_DIR WS
export ANDROID_HOME="$WS/toolchain/sdk"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/30.0.16248370"
export JAVA_HOME="$(ls -d "$WS"/toolchain/jdk-21* | head -1)"
export PATH="$ANDROID_HOME/cmake/3.31.6/bin:$ANDROID_HOME/platform-tools:$JAVA_HOME/bin:$PATH"

# minSdk 26 (Android 8.0): AAudio, GLES 3.2 on all real devices of that era, SDL2 baseline.
export ANDROID_API=26
export NDK_TOOLCHAIN="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake"
export JOBS="${JOBS:-$(nproc)}"

# Common CMake args for every cross-compiled target. $1 = ABI, $2 = install prefix.
android_cmake_args() {
    echo -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$NDK_TOOLCHAIN" \
        -DANDROID_ABI="$1" \
        -DANDROID_PLATFORM="android-$ANDROID_API" \
        -DANDROID_STL=c++_shared \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-Release}" \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DCMAKE_INSTALL_PREFIX="$2" \
        -DCMAKE_PREFIX_PATH="$2" \
        -DCMAKE_FIND_ROOT_PATH="$2"
}

# Emulator state stays inside the workspace (home partition is small).
export ANDROID_USER_HOME="$WS/toolchain/android-user"
export ANDROID_AVD_HOME="$WS/toolchain/avd"
export ANDROID_EMULATOR_HOME="$ANDROID_USER_HOME"
