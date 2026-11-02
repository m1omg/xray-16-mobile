#!/usr/bin/env bash
# Turn the built OpenXRay.app into an unsigned .ipa for sideloading (AltStore, SideStore,
# Sideloadly re-sign it with the player's Apple ID). Runs on macOS.
# Usage: scripts/package-ios.sh [output-dir]
set -euo pipefail
source "$(dirname "$0")/env.sh"

OUT="${1:-$WS/release}"
mkdir -p "$OUT" && OUT="$(cd "$OUT" && pwd)"
APP="$ENGINE_DIR/bin/ios-arm64/${BUILD_TYPE:-Release}/OpenXRay.app"
[[ -d "$APP" ]] || { echo "missing $APP, run build-engine-ios.sh first"; exit 1; }

# Engine resources the game data doesn't have; the app installs them into Documents.
rm -rf "$APP/gamedata" && mkdir -p "$APP/gamedata/shaders"
cp -R "$ENGINE_DIR/res/gamedata/shaders/gl" "$APP/gamedata/shaders/"
"$MOBILE_DIR/scripts/make-fsgame-soc.sh"
cp "$WS/data/fsgame-soc.ltx" "$APP/fsgame.ltx"

# Keep the symbol table for readable stack traces in the engine log, drop DWARF.
dsymutil "$APP/OpenXRay" -o "$OUT/OpenXRay.app.dSYM" 2> /dev/null || true
strip -S "$APP/OpenXRay"
# Ad-hoc signature: sideloading tools replace it, TrollStore-style installers accept it as is.
codesign --force --sign - --timestamp=none "$APP"

stage="$(mktemp -d)"
mkdir "$stage/Payload"
cp -R "$APP" "$stage/Payload/"
rm -f "$OUT/X-Ray-SoC.ipa"
(cd "$stage" && zip -qry "$OUT/X-Ray-SoC.ipa" Payload)
rm -rf "$stage"
echo "packaged $OUT/X-Ray-SoC.ipa ($(du -h "$OUT/X-Ray-SoC.ipa" | cut -f1))"
