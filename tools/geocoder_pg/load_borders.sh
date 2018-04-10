#!/bin/bash
set -e -u

DATABASE=borders
OSMCTOOLS="${OSMCTOOLS:-.}"
PLANET="${1:-$(ls planet-1*.o5m 2>/dev/null | head -n 1)}"
FILTERED="${FILTERED:-planet-borders.o5m}"
SCRIPT_PATH="$(cd "$(dirname "$0")"; pwd)"

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
  # "$OSMCTOOLS/osmfilter" "$PLANET" --keep="boundary=administrative =postal_code place=" -o="$FILTERED"
  "$OSMCTOOLS/osmfilter" "$PLANET" --keep="boundary=administrative =postal_code place= ( building= and addr:housenumber= )" -o="$FILTERED"
fi

TABLE="$(psql $DATABASE -qtAc "SELECT tablename FROM pg_tables WHERE tablename = 'osm_polygon'")"
if [ -n "$TABLE" ]; then
  echo "Renaming old table to osm_polygon_old"
  psql $DATABASE -qtAc "ALTER TABLE osm_polygon RENAME TO osm_polygon_old; DELETE FROM osm_polygon_old WHERE rank > 10"
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

if [ -n "$TABLE" ]; then
  echo "Restoring missing regions"
  psql $DATABASE <<EOF
    INSERT INTO osm_polygon (osm_id, admin_level, boundary, postal_code, name, place, population, tags, way)
    SELECT o.osm_id, o.admin_level, o.boundary, o.postal_code, o.name, o.place, o.population, o.tags, o.way
    FROM osm_polygon_old o LEFT JOIN osm_polygon p ON
      p.admin_level = old.admin_level AND
      p.place = old.place AND
      p.name = old.name AND
      p.way && ST_PointOnSurface(old.way)
    WHERE p.way IS NULL;

    DROP TABLE osm_polygon_old;
EOF
fi

echo "Postprocessing the database"
psql $DATABASE < "$SCRIPT_PATH/load_borders.sql"
