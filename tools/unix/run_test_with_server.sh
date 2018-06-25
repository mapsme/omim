#!/bin/bash

OMIM_PATH="$(cd "${OMIM_PATH:-$(dirname "$0")/../..}"; pwd)"

python $OMIM_PATH/tools/python/testserver.py \
       2>testserver.log &

# Run test command
$(echo "$1" | tr ";" " ")

# Kill server
[ -n "$(jobs -p)" ] && kill $(jobs -p)
