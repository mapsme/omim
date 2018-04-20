#!/usr/bin/env python3
import psycopg2
import json
import argparse


DB_NODE_BASE = 50*1000*1000*1000


class OsmIdCode:
    NODE = 0x4000000000000000
    WAY = 0x8000000000000000
    RELATION = 0xC000000000000000
    RESET = ~(NODE | WAY | RELATION)


if __name__ == '__main__':
    parser = argparse.ArgumentParser('Prepares a json feed with regions for geocoder')
    parser.add_argument('-d', '--database', default='borders',
                        help='Database name, default=borders')
    parser.add_argument('-t', '--table', default='osm_polygon',
                        help='Database table, default=osm_polygon')
    parser.add_argument('output', type=argparse.FileType('w'),
                        help='Output file, use "-" for stdout')
    options = parser.parse_args()

    cursor = psycopg2.connect('dbname='+options.database).cursor()
    cursor.execute('''select osm_id, level, name, rank,
                   ST_X(ST_PointOnSurface(way)), ST_Y(ST_PointOnSurface(way))
                   from {0} where rank <= 14 and name is not null order by rank'''.format(
                       options.table))
    for region in cursor.fetchall():
        osm_id, level, name, rank, lon, lat = region
        cursor.execute('''select level, name
                       from {0} p join {0}_geom g using (osm_id)
                       where ST_Intersects(g.way, ST_SetSRID(ST_MakePoint(%s, %s), 4326))
                       and rank < %s
                       '''.format(options.table), (lon, lat, rank))
        props = {level: name}
        for row in cursor:
            props[row[0]] = row[1]
        if osm_id < 0:
            rid = (-osm_id) | OsmIdCode.RELATION
        elif osm_id > DB_NODE_BASE:
            rid = (osm_id - DB_NODE_BASE) | OsmIdCode.NODE
        else:
            rid = osm_id | OsmIdCode.WAY
        if rid >= 2**63:
            # Negate as in int64_t
            rid = -1 - (rid ^ (2**64 - 1))
        feature = {'type': 'Feature', 'properties': {'name': name, 'rank': rank, 'address': props}}
        options.output.write(str(rid) + ' ' + json.dumps(feature, ensure_ascii=False) + '\n')
