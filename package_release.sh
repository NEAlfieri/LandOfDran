#!/usr/bin/env bash
#
# Packages a release binary along with the files it needs at runtime
# (Assets, Shaders, serverstart.lua) into a single tar.gz archive.
#
# By default the binary is built inside a Docker container pinned to an
# older Ubuntu baseline (see docker/release.Dockerfile), so its glibc and
# shared library requirements stay compatible with systems older than this
# machine — glibc compatibility only goes forward, not backward, so a
# binary built on a newer distro simply won't run on an older one.
#
# Several of the binary's shared library dependencies (Bullet, assimp,
# ENet, Lua, GLEW, SDL2) also use sonames that aren't stable across distro
# releases, so rather than relying on the player's package manager to have
# a matching version, those specific libraries get bundled into a lib/
# folder alongside a LandOfDran.sh launcher that points LD_LIBRARY_PATH at
# it (see scripts/bundle-libs.sh).
#
# Usage: ./package_release.sh [output_file]
#        ./package_release.sh --local [build_dir] [output_file]
#
#   (default)  Builds the release inside Docker. Requires Docker.
#   --local    Skips Docker and packages an already-built binary from
#              build_dir (default: cmake-build-release) instead. Faster for
#              local testing, but the result is only guaranteed to run on
#              systems with a glibc at least as new as this machine's.

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

VERSION="$(git rev-parse --short HEAD 2>/dev/null || date +%Y%m%d)"

STAGING_DIR="$(mktemp -d)"
trap 'rm -rf "$STAGING_DIR"' EXIT

PKG_DIR="$STAGING_DIR/LandOfDran"
mkdir -p "$PKG_DIR"

if [[ "${1:-}" == "--local" ]]; then
    shift
    BUILD_DIR="${1:-cmake-build-release}"
    [[ $# -gt 0 ]] && shift
    OUTPUT="${1:-LandOfDran-release-$VERSION.tar.gz}"

    BINARY="$BUILD_DIR/LandOfDran"
    if [[ ! -f "$BINARY" ]]; then
        echo "error: no binary found at '$BINARY' — build the project first (or pass the build dir as the first argument)" >&2
        exit 1
    fi

    cp "$BINARY" "$PKG_DIR/"
    ./scripts/bundle-libs.sh "$PKG_DIR/LandOfDran" "$PKG_DIR/lib"
else
    OUTPUT="${1:-LandOfDran-release-$VERSION.tar.gz}"

    if ! command -v docker >/dev/null; then
        echo "error: docker not found — install Docker, or pass --local to package an already-built binary instead" >&2
        exit 1
    fi

    echo "Building release inside Docker (docker/release.Dockerfile)..."
    docker build -f docker/release.Dockerfile -t landofdran-release-builder .

    CONTAINER_ID="$(docker create landofdran-release-builder)"
    docker cp "$CONTAINER_ID:/out/." "$PKG_DIR/"
    docker rm "$CONTAINER_ID" >/dev/null
fi

cp -r Assets "$PKG_DIR/"
cp -r Shaders "$PKG_DIR/"
cp serverstart.lua "$PKG_DIR/"

cat > "$PKG_DIR/LandOfDran.sh" <<'EOF'
#!/usr/bin/env bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$DIR/lib:${LD_LIBRARY_PATH:-}"
exec "$DIR/LandOfDran" "$@"
EOF
chmod +x "$PKG_DIR/LandOfDran.sh"

tar -czf "$OUTPUT" -C "$STAGING_DIR" LandOfDran

echo "Created $OUTPUT ($(du -h "$OUTPUT" | cut -f1))"

if command -v objdump >/dev/null; then
    MIN_GLIBC="$(objdump -T "$PKG_DIR/LandOfDran" "$PKG_DIR"/lib/*.so* 2>/dev/null | grep -oE 'GLIBC_[0-9]+\.[0-9]+' | sort -Vu | tail -1)"
    if [[ -n "$MIN_GLIBC" ]]; then
        echo "Requires glibc >= ${MIN_GLIBC#GLIBC_}"
    fi
fi
