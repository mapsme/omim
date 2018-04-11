#!/usr/bin/env python3
import psycopg2
import json
import multiprocessing
import argparse


def init_geocoder(options):
    global cursor, table
    cursor = psycopg2.connect('dbname='+options.database).cursor()
    table = options.table


def geocode(data):
    global cursor, table
    coords = data['geometry']['coordinates']
    cursor.execute('''select level, name, tags->'addr:street', tags->'addr:housenumber'
                   from {0} p join {0}_geom g using (osm_id)
                   where ST_Intersects(g.way, ST_SetSRID(ST_MakePoint(%s, %s), 4326))
                   '''.format(table), (coords[0], coords[1]))
    props = data['properties']['address']
    for row in cursor:
        if row[0] == 'building':
            if 'building' not in props and row[3]:
                props['building'] = row[3]
                if row[2]:
                    props['street'] = row[2]
        else:
            props[row[0]] = row[1]
    return data


def print_data(data):
    did = data['properties']['id']
    del data['properties']['id']
    options.output.write(did + ' ' + json.dumps(data, ensure_ascii=False) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser('Geocodes a list of points in a geojson lines')
    parser.add_argument('input', type=argparse.FileType('r'),
                        help='Source file')
    parser.add_argument('-d', '--database', default='borders',
                        help='Database name, default=borders')
    parser.add_argument('-t', '--table', default='osm_polygon',
                        help='Database table, default=osm_polygon')
    parser.add_argument('output', type=argparse.FileType('w'),
                        help='Output file, use "-" for stdout')
    options = parser.parse_args()

    with multiprocessing.Pool(initializer=init_geocoder, initargs=(options,)) as pool:
        for data in pool.imap_unordered(geocode, (json.loads(line) for line in options.input), 100):
            if 'building' in data['properties']['address']:
                print_data(data)
