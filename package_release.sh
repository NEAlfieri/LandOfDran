#!/usr/bin/env bash
#
# Packages the latest release binary along with the files it needs at
# runtime (Assets, Shaders, serverstart.lua) into a single tar.gz archive.
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
mkdir -p "$PKG_DIR"

cp "$BINARY" "$PKG_DIR/"
cp -r Assets "$PKG_DIR/"
cp -r Shaders "$PKG_DIR/"
cp serverstart.lua "$PKG_DIR/"

tar -czf "$OUTPUT" -C "$STAGING_DIR" LandOfDran

echo "Created $OUTPUT ($(du -h "$OUTPUT" | cut -f1))"
