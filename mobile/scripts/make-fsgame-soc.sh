#!/usr/bin/env bash
# Derive the SoC fsgame.ltx from OpenXRay's (CoP-layout) one.
# SoC keeps its configs in gamedata\config\ rather than configs\. Weathers stay on the
# CoP paths on purpose: they don't exist in SoC data, so the engine falls back to the
# ShoC-style [weathers]/[weather_effects] tables from system.ltx.
set -euo pipefail
source "$(dirname "$0")/env.sh"
sed -E 's#(\$game_config\$ .*\$game_data\$\| *)configs\\#\1config\\#' \
    "$ENGINE_DIR/res/fsgame.ltx" > "$WS/data/fsgame-soc.ltx"
