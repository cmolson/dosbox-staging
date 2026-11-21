#!/usr/bin/env bash
# Usage: kiosk/build/build.sh [release|debug]
# Output: build/<type>-linux/dosbox
set -euo pipefail

cd "$(dirname "$0")/../.."
SRC=$(pwd)
TYPE=${1:-release}
IMAGE=dosbox-kiosk-build:22.04

docker build -t "$IMAGE" -f kiosk/build/Dockerfile .

mkdir -p "$HOME/.cache/dosbox-kiosk/vcpkg-bin" "$HOME/.cache/dosbox-kiosk/vcpkg-dl"

docker run --rm \
    --user "$(id -u):$(id -g)" \
    -e HOME=/tmp \
    -e VCPKG_DEFAULT_BINARY_CACHE=/cache/bin \
    -e VCPKG_DOWNLOADS=/cache/dl \
    -v "$HOME/.cache/dosbox-kiosk/vcpkg-bin:/cache/bin" \
    -v "$HOME/.cache/dosbox-kiosk/vcpkg-dl:/cache/dl" \
    -v "$SRC:/src" -w /src \
    "$IMAGE" bash -c "
        set -e
        cmake --preset ${TYPE}-linux-vcpkg -DOPT_TESTS=OFF
        cmake --build --preset ${TYPE}-linux-vcpkg
    "

echo "Built: $SRC/build/${TYPE}-linux/dosbox"
