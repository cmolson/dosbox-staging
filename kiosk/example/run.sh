#!/usr/bin/env sh
# Runs the example without reading or creating your normal DOSBox config.
cd "$(dirname "$0")" || exit 1
exec ../dosbox --noprimaryconf "$@"
