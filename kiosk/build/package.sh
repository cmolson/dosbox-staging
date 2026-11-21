#!/usr/bin/env bash
# Usage: kiosk/build/package.sh OFFICIAL_RELEASE_DIR EXAMPLE_DIR
#
# OFFICIAL_RELEASE_DIR is an unpacked official release of the same version,
# for the lib/ and plugins/ folders that upstream CI adds. EXAMPLE_DIR has
# kiosk-games.conf, images/ and drive_c/ for kiosk-example/.
set -euo pipefail

cd "$(dirname "$0")/../.."
OFFICIAL=$1
EXAMPLE=$2

VERSION=$(git describe --tags --always | sed 's/^v//')
NAME="dosbox-staging-kiosk-linux-x86_64-${VERSION}"
OUT=dist/$NAME

rm -rf "$OUT" && mkdir -p dist
./scripts/packaging/create-package.sh -p linux build/release-linux "$OUT"

cp -r "$OFFICIAL/lib" "$OFFICIAL/plugins" "$OUT/"

mkdir -p "$OUT/kiosk-example"
cp -r "$EXAMPLE/drive_c" "$EXAMPLE/images" "$EXAMPLE/kiosk-games.conf" "$OUT/kiosk-example/"
cp kiosk/example/dosbox.conf kiosk/example/run.sh "$OUT/kiosk-example/"
cp kiosk/README.md "$OUT/README-KIOSK.md"

tar -C dist -cJf "dist/$NAME.tar.xz" "$NAME"
echo "Package: $(pwd)/dist/$NAME.tar.xz"
