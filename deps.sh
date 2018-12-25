#!/bin/bash
set -euo pipefail

TIDY_VERSION=5.6.0
TIDY_HASH=08a63bba3d9e7618d1570b4ecd6a7daa83c8e18a41c82455b6308bc11fe34958
TIDY_DIR=tidy-html5-$TIDY_VERSION
TIDY_PKG=$TIDY_DIR.tar.gz
TIDY_URL=https://github.com/htacg/tidy-html5/archive/$TIDY_VERSION.tar.gz

main() {
    [ -e deps ] || mkdir deps
    cd deps/
    [ -e include ] || mkdir include
    [ -e lib ] || mkdir lib
    tidy
}

tidy() {
    download "$TIDY_DIR" "$TIDY_PKG" "$TIDY_URL" "$TIDY_HASH"
    (cd "$TIDY_DIR" && cmake CMakeLists.txt && make -j "$(nproc)")
    link include ../ "$TIDY_DIR/include"/*
    link lib ../ "$TIDY_DIR"/libtidy*.so*
}

download() {
    local dir=$1 pkg=$2 url=$3 hash=$4
    [ -e "$pkg" ] || curl --location --output "$pkg" "$url"
    echo "$hash $pkg" | sha256sum --check
    [ -e "$dir" ] || tar -xf "$pkg"
}

link() {
    local dst=$1 pref=$2; shift 2
    local src_f dst_f
    for src_f; do
        dst_f=$dst/${src_f##*/}
        [ -e "$dst_f" ] || ln -s "$pref$src_f" "$dst_f"
    done
}

main
