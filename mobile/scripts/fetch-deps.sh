#!/usr/bin/env bash
# Fetch the pinned third-party dependency sources into $WS/deps-src (shared by Android and iOS).
set -euo pipefail
source "$(dirname "$0")/env.sh"

SRC="$WS/deps-src"
mkdir -p "$SRC"

fetch() {
    local dir="$1" url="$2" tag="$3"
    if [[ -d "$SRC/$dir" ]]; then
        echo "$dir: present"
        return
    fi
    git -c advice.detachedHead=false clone -q --depth 1 --branch "$tag" "$url" "$SRC/$dir"
    echo "$dir: $tag"
}

fetch SDL2          https://github.com/libsdl-org/SDL.git               release-2.32.10
fetch openal-soft   https://github.com/kcat/openal-soft.git              1.25.2
fetch ogg           https://github.com/xiph/ogg.git                      v1.3.6
fetch vorbis        https://github.com/xiph/vorbis.git                   v1.3.7
fetch theora        https://github.com/xiph/theora.git                   v1.2.0
fetch libjpeg-turbo https://github.com/libjpeg-turbo/libjpeg-turbo.git   3.2.1

if [[ ! -d "$SRC/lzo-2.10" ]]; then
    curl -sSL https://www.oberhumer.com/opensource/lzo/download/lzo-2.10.tar.gz | tar xz -C "$SRC"
    echo "lzo-2.10: fetched"
fi
