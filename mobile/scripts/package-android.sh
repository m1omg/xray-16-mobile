#!/usr/bin/env bash
# Collect the engine + dependency .so files into the Gradle project and build the APK.
# Usage: [STATIC=1] scripts/package-android.sh <abi>... [--debug-symbols]
set -euo pipefail
source "$(dirname "$0")/env.sh"

GRADLE_VERSION=9.8.2
GRADLE="$WS/toolchain/gradle-$GRADLE_VERSION/bin/gradle"
LLVM="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64"

keep_symbols=0
abis=()
for arg in "$@"; do
    case "$arg" in
        --debug-symbols) keep_symbols=1 ;;
        *) abis+=("$arg") ;;
    esac
done
[[ ${#abis[@]} -gt 0 ]] || { echo "usage: $0 <abi>... [--debug-symbols]"; exit 1; }

if [[ ! -x "$GRADLE" ]]; then
    curl -sSL -o "$WS/dl/gradle-$GRADLE_VERSION-bin.zip" \
        "https://services.gradle.org/distributions/gradle-$GRADLE_VERSION-bin.zip"
    unzip -q -o "$WS/dl/gradle-$GRADLE_VERSION-bin.zip" -d "$WS/toolchain"
fi

printf "sdk.dir=%s\n" "$ANDROID_HOME" > "$MOBILE_DIR/android/local.properties"

# Only the ABIs asked for this time, not leftovers from an earlier run
rm -rf "$MOBILE_DIR/android/app/jniLibs"

for abi in "${abis[@]}"; do
    case "$abi" in
        arm64-v8a) triple=aarch64-linux-android ;;
        x86_64)    triple=x86_64-linux-android ;;
        *) echo "unsupported ABI $abi"; exit 1 ;;
    esac
    out="$MOBILE_DIR/android/app/jniLibs/$abi"
    mkdir -p "$out"

    variant=android-"$abi"; [[ "${STATIC:-0}" == 1 ]] && variant+=-static
    libs=("$ENGINE_DIR"/bin/"$variant"/"${BUILD_TYPE:-Release}"/lib*.so
          "$WS/prefix/$abi/lib/libSDL2.so"
          "$WS/prefix/$abi/lib/libopenal.so"
          "$LLVM/sysroot/usr/lib/$triple/libc++_shared.so")
    for lib in "${libs[@]}"; do
        if [[ $keep_symbols == 1 ]]; then
            cp "$lib" "$out/"
        else
            "$LLVM/bin/llvm-strip" --strip-debug -o "$out/$(basename "$lib")" "$lib"
        fi
    done
    echo "$abi: $(ls "$out" | wc -l) libraries, $(du -sh "$out" | cut -f1)"
done

# Engine resources shipped in the APK and unpacked by StalkerActivity on install
assets="$MOBILE_DIR/android/app/engineAssets"
rm -rf "$assets" && mkdir -p "$assets/gamedata/shaders"
cp -r "$ENGINE_DIR/res/gamedata/shaders/gl" "$assets/gamedata/shaders/"
"$MOBILE_DIR/scripts/make-fsgame-soc.sh"
cp "$WS/data/fsgame-soc.ltx" "$assets/fsgame.ltx"

"$GRADLE" -p "$MOBILE_DIR/android" --no-daemon -q assembleDebug
ls -la "$MOBILE_DIR"/android/app/build/outputs/apk/debug/*.apk
