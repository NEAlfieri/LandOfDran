#!/usr/bin/env bash
#
# Copies a binary's shared library dependencies that don't have a stable
# soname across distro releases (Bullet, assimp, ENet, Lua, GLEW, SDL2, and
# assimp's own transitive deps) into a lib/ directory next to it, so the
# binary doesn't depend on the host having a matching package version
# installed. Must be run in an environment where `ldd` resolves against the
# libraries the binary was actually built against (i.e. on the build
# machine itself, or inside the container that built it) — running it
# against libraries from an unrelated system defeats the point.
#
# Usage: bundle-libs.sh <binary> <output-lib-dir>

set -euo pipefail

BINARY="$1"
LIB_DIR="$2"
mkdir -p "$LIB_DIR"

BUNDLE_LIB_PREFIXES=(
    libSDL2-2.0.so
    libassimp.so
    liblua5.4.so
    libGLEW.so
    libBulletDynamics.so
    libBulletCollision.so
    libLinearMath.so
    libenet.so
    libdraco.so
    libminizip.so
    libpugixml.so
)

while read -r soname respath; do
    for prefix in "${BUNDLE_LIB_PREFIXES[@]}"; do
        if [[ "$soname" == "$prefix"* && -f "$respath" ]]; then
            cp -L "$respath" "$LIB_DIR/$soname"
            break
        fi
    done
done < <(ldd "$BINARY" | awk '/=>/{print $1, $3}')
