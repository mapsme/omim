\! echo "Dropping lines"
DROP TABLE osm_line;
DROP TABLE osm_roads;


\! echo "Adding centroids to polygons"
ALTER TABLE osm_polygon ADD COLUMN centroid geometry(Point, 4326);
UPDATE osm_polygon SET centroid = ST_Centroid(way) WHERE place IS NOT NULL;


\! echo "Splitting big polygons"
drop table osm_polygon_geom;
CREATE TABLE osm_polygon_geom AS
SELECT osm_id, ST_Subdivide(way) AS way
FROM osm_polygon;


\! echo "Creating useful indices"
CREATE INDEX ON osm_polygon_geom USING GIST (way);
CREATE INDEX ON osm_polygon_geom (osm_id);
CREATE INDEX ON osm_polygon (osm_id);
CREATE INDEX ON osm_polygon (name);
CREATE INDEX ON osm_point (name);
CREATE INDEX ON osm_point (place);


\! echo "Merging place polygons with place nodes"
WITH places AS (
  SELECT poly.osm_id, pt.tags, pt.osm_id AS point_id
  FROM osm_polygon poly, osm_point pt, osm_polygon_geom geom
  WHERE
    geom.osm_id = poly.osm_id
    AND poly.place = pt.place AND poly.name = pt.name
    AND geom.way && pt.way AND ST_Intersects(geom.way, pt.way)
    AND poly.place IS NOT NULL AND poly.name IS NOT NULL
), places2 AS (
  DELETE FROM osm_point
  USING places
  WHERE osm_point.osm_id = point_id
)
UPDATE osm_polygon poly
SET tags = poly.tags || places.tags
FROM places
WHERE poly.osm_id = places.osm_id;


\! echo "Creating polygons from unmatched place nodes"
WITH inserted AS (
INSERT INTO osm_polygon
  (osm_id, place, name, tags, way, centroid)
SELECT
  -- Max way id is 565 mln as of March 2018
  osm_id + 50*1000*1000*1000, place, name, tags,
  ST_Buffer(way, CASE
    -- Based on average radiuses of OSM place polygons
    WHEN place = 'city' THEN 0.078
    WHEN place = 'town' THEN 0.033
    WHEN place = 'village' THEN 0.013
    WHEN place = 'hamlet' THEN 0.0067
    WHEN place = 'suburb' THEN 0.016
    WHEN place = 'neighbourhood' THEN 0.0035
  END, 2) as way, way as centroid
FROM
  osm_point
WHERE
  name IS NOT NULL
  AND place IN ('city', 'town', 'village',
    'hamlet', 'suburb', 'neighbourhood')
RETURNING osm_id, way
)
INSERT INTO osm_polygon_geom (osm_id, way)
SELECT osm_id, ST_Subdivide(way)
FROM inserted;


\! echo "Dropping points table"
DROP TABLE osm_point;


\! echo "Adding level labels"
ALTER TABLE osm_polygon ADD COLUMN level VARCHAR(12);
UPDATE osm_polygon SET level = CASE
  WHEN place IN ('city', 'town', 'village', 'hamlet') THEN 'locality'
  WHEN place IN ('suburb', 'neighbourhood') THEN 'suburb'
  WHEN place IN ('locality', 'isolated_dwelling') THEN 'sublocality'
  WHEN boundary = 'administrative' AND admin_level = '2' THEN 'country'
  WHEN boundary = 'administrative' AND admin_level = '4' THEN 'region'
  WHEN boundary = 'administrative' AND admin_level = '6' THEN 'subregion'
  WHEN building IS NOT NULL THEN 'building'
END;


\! echo "Deleting polygons without a level"
WITH deleted AS (
  DELETE FROM osm_polygon
  WHERE level IS NULL
  RETURNING osm_id AS del_id
)
DELETE FROM osm_polygon_geom
USING deleted
WHERE osm_id = del_id;


\! echo "Adding place ranks"
ALTER TABLE osm_polygon ADD COLUMN rank SMALLINT;
UPDATE osm_polygon SET rank = CASE
  WHEN boundary = 'administrative' AND admin_level IN ('2', '3', '4', '5', '6', '7', '8') THEN admin_level::int
  WHEN place = 'city' THEN 10
  WHEN place = 'town' THEN 11
  WHEN place = 'village' THEN 13
  WHEN place = 'suburb' THEN 14
  WHEN place = 'neighbourhood' THEN 15
  WHEN place = 'hamlet' THEN 17
  WHEN place = 'locality' THEN 19
  WHEN place = 'isolated_dwelling' THEN 20
  WHEN building IS NOT NULL THEN 30
END


\! echo "Statistics"
SELECT level, count(*) FROM osm_polygon WHERE level IS NOT NULL GROUP BY level;
