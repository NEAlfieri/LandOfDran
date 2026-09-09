#!/usr/bin/env bash
#
# Packages the latest release binary along with the files it needs at
# runtime (Assets, Shaders, serverstart.lua) into a single tar.gz archive.
#
# Some of the binary's shared library dependencies (Bullet, assimp, etc.)
# use sonames that change between distro releases, so relying on the
# player's system package manager for those is fragile — a "libbullet3.24"
# on the build machine might be "libbullet3.06" or similar elsewhere, which
# won't satisfy the exact soname the binary was linked against. To avoid
# that, this script bundles those specific libraries into a lib/ folder and
# ships a launcher script that points LD_LIBRARY_PATH at it. Libraries tied
# to the host's graphics driver / display server / audio daemon (OpenGL,
# X11, Wayland, ALSA, PulseAudio, ...) are left to the system, since they
# must match the host anyway and are near-universally already present.
#
# Usage: ./package_release.sh [build_dir] [output_file]
#   build_dir   Directory containing the built LandOfDran binary
#               (default: cmake-build-release)
#   output_file Name of the archive to create
#               (default: LandOfDran-release-<git short hash or date>.tar.gz)

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

BUILD_DIR="${1:-cmake-build-release}"
BINARY="$BUILD_DIR/LandOfDran"

if [[ ! -f "$BINARY" ]]; then
    echo "error: no binary found at '$BINARY' — build the project first (or pass the build dir as the first argument)" >&2
    exit 1
fi

VERSION="$(git rev-parse --short HEAD 2>/dev/null || date +%Y%m%d)"
OUTPUT="${2:-LandOfDran-release-$VERSION.tar.gz}"

STAGING_DIR="$(mktemp -d)"
trap 'rm -rf "$STAGING_DIR"' EXIT

PKG_DIR="$STAGING_DIR/LandOfDran"
mkdir -p "$PKG_DIR/lib"

cp "$BINARY" "$PKG_DIR/"
cp -r Assets "$PKG_DIR/"
cp -r Shaders "$PKG_DIR/"
cp serverstart.lua "$PKG_DIR/"

# Libraries whose soname isn't stable across distro releases, so we bundle
# our own copy rather than trust the player's package manager to have a
# matching version.
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
            cp -L "$respath" "$PKG_DIR/lib/$soname"
            break
        fi
    done
done < <(ldd "$BINARY" | awk '/=>/{print $1, $3}')

cat > "$PKG_DIR/LandOfDran.sh" <<'EOF'
#!/usr/bin/env bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$DIR/lib:${LD_LIBRARY_PATH:-}"
exec "$DIR/LandOfDran" "$@"
EOF
chmod +x "$PKG_DIR/LandOfDran.sh"

tar -czf "$OUTPUT" -C "$STAGING_DIR" LandOfDran

echo "Created $OUTPUT ($(du -h "$OUTPUT" | cut -f1))"
