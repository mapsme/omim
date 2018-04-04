#!/bin/bash
set -e -u

DATABASE=borders
OSMCTOOLS="${OSMCTOOLS:-.}"
PLANET="${1:-$(ls planet-1*.o5m 2>/dev/null | head -n 1)}"
FILTERED="${FILTERED:-planet-borders.o5m}"

if ! command -v osm2pgsql > /dev/null; then
  echo "Please install osm2pgsql"
  exit 3
fi
if ! command -v psql > /dev/null; then
  echo "Please install PostgreSQL"
  exit 3
fi
if ! psql -lqtA | grep -q "^$DATABASE|"; then
  echo "Please create $DATABASE database with postgis and hstore extensions"
  exit 3
fi
if [ ! -f "$PLANET" ]; then
  echo "Usage: $0 <planet.o5m>"
  exit 1
fi

if [ ! -x "$OSMCTOOLS/osmfilter"  ]; then
  echo "Compiling osmfilter"
  SCRIPT_PATH="$(cd "$(dirname "$0")"; pwd)"
  cc -x c -O3 "$SCRIPT_PATH/../osmctools/osmfilter.c"  -o "$OSMCTOOLS/osmfilter"
fi

echo "Filtering $PLANET"
if [ -f "$FILTERED" ]; then
  echo "Filtered file already exists, skipping it (make sure it's not too old!)"
  ls -l "$FILTERED"
else
  "$OSMCTOOLS/osmfilter" "$PLANET" --keep="boundary=administrative =postal_code place= ( building= and addr:housenumber= )" -o="$FILTERED"
fi

echo "Loading $FILTERED into PostgreSQL database"
TMP_STYLE="$(mktemp)"
cat > "$TMP_STYLE" <<EOF
way      admin_level text polygon
way      boundary    text polygon
way      postal_code text polygon
way      building    text polygon
node,way name        text polygon
node,way place       text polygon
EOF

osm2pgsql --create --slim --drop --number-processes 8 \
  --database $DATABASE --prefix osm --unlogged \
  --style "$TMP_STYLE" --hstore --latlong \
  --cache 8000 --multi-geometry "$FILTERED"
rm "$TMP_STYLE"

echo "Postprocessing the database"
psql $DATABASE < load_borders.sql
