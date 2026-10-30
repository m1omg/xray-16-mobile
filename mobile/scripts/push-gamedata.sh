#!/usr/bin/env bash
# Copy the user's SoC archives into the app's external files dir on the device.
# Usage: scripts/push-gamedata.sh [game-install-dir]
#
# Only gamedata.db* goes over adb: engine resources (GL shaders, fsgame.ltx) ship in the APK
# and the app unpacks them itself. That matters because directories created by `adb shell`
# inside Android/data/<pkg> are owned by the shell user and the app can't opendir() them.
set -euo pipefail
source "$(dirname "$0")/env.sh"

GAME="${1:-$WS/../S.T.A.L.K.E.R. Shadow of Chernobyl}"
PKG=org.xraymobile.soc
DEST=/sdcard/Android/data/$PKG/files

# Let the app create its external files dir (and unpack its resources) first.
adb shell am start -W -n "$PKG/.StalkerActivity" > /dev/null
adb shell am force-stop "$PKG"

for f in "$GAME"/gamedata.db?; do
    name=$(basename "$f")
    remote_size=$(adb shell "stat -c %s $DEST/$name 2>/dev/null" || true)
    if [[ "$remote_size" != "$(stat -c %s "$f")" ]]; then
        adb push "$f" "$DEST/$name" | tail -1
    fi
done
adb shell "ls $DEST"
