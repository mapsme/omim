#!/bin/bash
set -e -u -x

SCRIPTS_PATH="$(dirname "$0")"
"$SCRIPTS_PATH/generate_symbols.sh"
"$SCRIPTS_PATH/generate_drules.sh"
ALL_DRULES="$(find "$SCRIPTS_PATH/../../data" -name drules_proto_*.bin -d 1 -print0 | xargs -0 "$SCRIPTS_PATH/../python/stylesheet/color_matrix_builder.py")"


