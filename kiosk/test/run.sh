#!/usr/bin/env bash
# Usage: kiosk/test/run.sh EXAMPLE_DIR screenshots|record
#
# Results go to EXAMPLE_DIR/shots or EXAMPLE_DIR/demo.
set -euo pipefail

cd "$(dirname "$0")/../.."
EXAMPLE=$(realpath "$1")
SCRIPT=$2
IMAGE=dosbox-kiosk-test:22.04

docker build -q -t "$IMAGE" kiosk/test > /dev/null

docker run --rm \
    --user "$(id -u):$(id -g)" \
    -e HOME=/tmp \
    -v "$EXAMPLE:/test" \
    -v "$(pwd)/build/release-linux:/dosbox:ro" \
    -v "$(pwd)/resources:/dosbox/resources:ro" \
    -v "$(pwd)/kiosk/test:/scripts:ro" \
    -w /test \
    "$IMAGE" "/scripts/$SCRIPT.sh"
