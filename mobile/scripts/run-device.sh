#!/usr/bin/env bash
# Install the debug APK, launch it, and collect the engine log + a screenshot.
# Usage: [EXTRA_ARGS="-start ..."] scripts/run-device.sh [seconds-to-run]   (ANDROID_SERIAL selects the device)
set -euo pipefail
source "$(dirname "$0")/env.sh"

PKG=org.xraymobile.soc
SECONDS_TO_RUN="${1:-40}"
OUT="$WS/logs/run-$(date +%H%M%S)"
mkdir -p "$OUT"

adb install -r -g "$MOBILE_DIR/android/app/build/outputs/apk/debug/app-debug.apk" > /dev/null
adb shell am force-stop "$PKG"
adb logcat -c
if [[ -n "${EXTRA_ARGS:-}" ]]; then
    # Single-quoted for the device shell: -start args contain parentheses
    adb shell am start -n "$PKG/.StalkerActivity" --es args "'$EXTRA_ARGS'" > /dev/null
else
    adb shell am start -n "$PKG/.StalkerActivity" > /dev/null
fi

sleep "$SECONDS_TO_RUN"

adb exec-out screencap -p > "$OUT/screen.png" || true
adb logcat -d -v brief > "$OUT/logcat-full.txt"
# Engine log lines (tag "xray"), SDL, and native crash reports
grep -E "/(xray|SDL|SDL/APP|DEBUG|libc|AndroidRuntime) *\(" "$OUT/logcat-full.txt" > "$OUT/logcat.txt" || true
pid=$(adb shell pidof "$PKG" || true)
echo "out: $OUT  pid: ${pid:-not running}"
tail -n 40 "$OUT/logcat.txt"
