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

echo "Filtering $PLANET"
if [ -f "$FILTERED" ]; then
  echo "Filtered file already exists, skipping it (make sure it's not too old!)"
  ls -l "$FILTERED"
else
  if [ ! -x "$OSMCTOOLS/osmfilter"  ]; then
    echo "Compiling osmfilter"
    SCRIPT_PATH="$(cd "$(dirname "$0")"; pwd)"
    cc -x c -O3 "$SCRIPT_PATH/../osmctools/osmfilter.c"  -o "$OSMCTOOLS/osmfilter"
  fi

  "$OSMCTOOLS/osmfilter" "$PLANET" --keep="boundary=administrative =postal_code place=" -o="$FILTERED"
  # "$OSMCTOOLS/osmfilter" "$PLANET" --keep="boundary=administrative =postal_code place= ( building= and addr:housenumber= )" -o="$FILTERED"
fi

TABLE="$(psql $DATABASE -qtAc "SELECT tablename FROM pg_tables WHERE tablename = 'osm_polygon'")"
if [ -n "$TABLE" ]; then
  echo "Renaming old table to osm_polygon_old"
  psql $DATABASE -qtAc "ALTER TABLE osm_polygon RENAME TO osm_polygon_old; ALTER INDEX osm_polygon_index RENAME TO osm_polygon_old_index; DELETE FROM osm_polygon_old WHERE rank > 6"
  echo "Deleting old tables and indexes"
  psql $DATABASE -qtAc "DROP TABLE IF EXISTS osm_polygon_geom; DROP INDEX IF EXISTS osm_polygon_name_idx, osm_polygon_osm_id_idx, osm_polygon_rank_idx"
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
  BEFORE="$(psql $DATABASE -qtAc "SELECT count(*) FROM osm_polygon WHERE admin_level in ('2', '3', '4', '5', '6')")"
  psql $DATABASE <<EOF
    INSERT INTO osm_polygon (osm_id, admin_level, boundary, postal_code, name, place, tags, way)
    SELECT o.osm_id, o.admin_level, o.boundary, o.postal_code, o.name, o.place, o.tags, o.way
    FROM osm_polygon_old o LEFT JOIN osm_polygon p ON
      p.admin_level = o.admin_level AND
      p.way && o.centroid
    WHERE p.way IS NULL;

    DROP TABLE osm_polygon_old;
EOF
  AFTER="$(psql $DATABASE -qtAc "SELECT count(*) FROM osm_polygon WHERE admin_level in ('2', '3', '4', '5', '6')")"
  echo "Added $(($AFTER-$BEFORE)) borders"
fi

echo "Postprocessing the database"
psql $DATABASE < "$SCRIPT_PATH/load_borders.sql"
