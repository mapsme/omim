#!/usr/bin/env python3
import psycopg2
import json
import multiprocessing
import argparse


def init_geocoder(database):
    global cursor
    cursor = psycopg2.connect('dbname='+database).cursor()


def geocode(data):
    global cursor
    coords = data['geometry']['coordinates']
    cursor.execute('''select level, name, tags->'addr:street', tags->'addr:housenumber'
                   from osm_polygon p join osm_polygon_geom g using (osm_id)
                   where ST_Intersects(g.way, ST_SetSRID(ST_MakePoint(%s, %s), 4326))
                   ''', (coords[0], coords[1]))
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
    parser.add_argument('output', type=argparse.FileType('w'),
                        help='Output file, use "-" for stdout')
    options = parser.parse_args()

    with multiprocessing.Pool(initializer=init_geocoder, initargs=(options.database,)) as pool:
        for data in pool.imap_unordered(geocode, (json.loads(line) for line in options.input), 100):
            if 'building' in data['properties']['address']:
                print_data(data)
