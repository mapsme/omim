#!/usr/bin/env python3
import psycopg2
import psycopg2.extras
import json
import logging


def parse_name_and_wiki(row, tags):
    for k, v in tags.items():
        if k[:5] == 'name:':
            row['name_' + k[5:]] = v
        elif k in ('wikipedia', 'wikidata', 'int_name'):
            row[k] = v

logging.basicConfig(level=logging.INFO, datefmt='%H:%M:%S', format='%(asctime)s %(message)s')
conn = psycopg2.connect('dbname=borders')
psycopg2.extras.register_hstore(conn)
cur = conn.cursor()

logging.info('Prefetching countries and regions')
cur.execute("SELECT rank, osm_id, name, tags FROM osm_polygon WHERE rank = 2 OR rank = 4")
regions = {}
countries = {}
for row in cur:
    if row[0] == 4 and row[1] not in regions:
        regions[row[1]] = {}
        regions[row[1]]['name'] = row[2]
        regions[row[1]]['iso'] = (row[3].get('ISO3166-2') or
                                  row[3].get('int_ref'))
        parse_name_and_wiki(regions[row[1]], row[3])
    if row[0] == 2 and row[1] not in countries:
        countries[row[1]] = {}
        countries[row[1]]['name'] = row[2]
        countries[row[1]]['iso'] = (row[3].get('ISO3166-1:alpha2') or
                                    row[3].get('ISO3166-1') or
                                    row[3].get('int_ref'))
        parse_name_and_wiki(countries[row[1]], row[3])

logging.info('Querying places')
cur.execute("""
SELECT
  COALESCE(pl.node_id, pl.osm_id) AS id, pl.place, pl.name,
  ST_X(pl.centroid) as lon, ST_Y(pl.centroid) as lat,
  pl.tags,
  adm4.osm_id, adm2.osm_id
FROM
  osm_polygon pl
  LEFT JOIN osm_polygon adm4 ON ST_Intersects(adm4.way, pl.centroid) AND adm4.rank = 4
  LEFT JOIN osm_polygon adm2 ON ST_Intersects(adm2.way, pl.centroid) AND adm2.rank = 2
WHERE
  pl.place IN ('city', 'town', 'village')
  AND adm2.osm_id IS NOT NULL
            """)


logging.info('Populating countries, regions and places dicts')
places = {}
unused_regions = set(regions.keys())
unused_countries = set(countries.keys())
while True:
    try:
        row = next(cur)
    except StopIteration:
        break
    except psycopg2.InterfaceError as e:
        logging.error('Error processing a psycopg2 row: %s', str(e))
        continue
    if row[0] not in places:
        places[row[0]] = {}
        places[row[0]]['place'] = row[1]
        places[row[0]]['name'] = row[2]
        places[row[0]]['lon'] = row[3]
        places[row[0]]['lat'] = row[4]
        places[row[0]]['region'] = row[6]
        places[row[0]]['country'] = row[7]
        if row[6]:
            unused_regions.discard(row[6])
            regions[row[6]]['country'] = row[7]
        parse_name_and_wiki(places[row[0]], row[5])
        unused_countries.discard(row[7])
    elif row[7] < places[row[0]]['country']:
        places[row[0]]['region'] = row[6]
        places[row[0]]['country'] = row[7]
        if row[6]:
            unused_regions.discard(row[6])
            regions[row[6]]['country'] = row[7]
        unused_countries.discard(row[7])


logging.info('Cleaning up %s unused countries and %s unused regions',
             len(unused_countries), len(unused_regions))
for k in unused_countries:
    del countries[k]
for k in unused_regions:
    del regions[k]


logging.info('Filtering')

# Delete places with incorrect coordinates
dp = 0
for p in list(places.keys()):
    if (not isinstance(places[p]['lat'], (int, float)) or
            not isinstance(places[p]['lon'], (int, float)) or
            not places[p]['name']):
        dp += 1
        del places[p]
if dp:
    logging.warn('Deleted %s places with bad coordinates', dp)

# Find ids of regions and countries that are not referenced by places
reg_ids = set(regions.keys())
c_ids = set(countries.keys())
for k in places:
    reg_ids.discard(places[k]['region'])
    c_ids.discard(places[k]['country'])
if reg_ids or c_ids:
    logging.warn('Found %s regions and %s countries not referenced by places',
                 len(reg_ids), len(c_ids))

# Delete said regions and countries
for reg_id in reg_ids:
    del regions[reg_id]
for k in regions:
    c_ids.discard(regions[k]['country'])
for c_id in c_ids:
    del countries[c_id]

# Find countries with positive ids and delete them, deleting referencing regions and places
rdel = set()
country_ways = list(filter(lambda x: x > 0, countries))
if country_ways:
    logging.info('Removing %s countries drawn as ways and their dependents', len(country_ways))
for k in country_ways:
    c = list(filter(lambda x: x < 0 and countries[x]['name'] == countries[k]['name'], countries))
    if c:
        c = c[0]
    for r in list(filter(lambda x: regions[x].get('country', 0) == k, regions)):
        if c:
            regions[r]['country'] = c
        else:
            rdel.add(r)
            del regions[r]
    for p in list(filter(lambda x: places[x].get('country', 0) == k, places)):
        if c:
            places[p]['country'] = c
        else:
            del places[p]
    del countries[k]

# Find and delete places and regions referencing unlisted regions and countries
dp = 0
for p in list(filter(lambda x: places[x].get('region') and
                     places[x].get('region') not in regions, places)):
    dp += 1
    logging.warn('Region %s not listed for place %s', places[p].get('region'), p)
    del places[p]
for p in list(filter(lambda x: places[x].get('country', 0) not in countries, places)):
    dp += 1
    logging.warn('Country %s not listed for place %s', places[p].get('country'), p)
    del places[p]
reg_ids = set()
for p in list(filter(lambda x: regions[x].get('country', 0) not in countries, regions)):
    reg_ids.add(int(p))
    del regions[p]
if dp:
    logging.info('Deleted %s places referencing unlisted regions or countries', dp)
# We deleted some regions, and they were referenced by places
for p in list(places.keys()):
    if places[p]['region'] in reg_ids:
        del places[p]['region']

# Find and fix discrepancies between place->country and place->region->country
for p in places:
    if places[p].get('region') and places[p].get('country'):
        places[p]['country'] = regions[places[p]['region']]['country']

logging.info('Done. Countries: %s, regions: %s, places: %s',
             len(countries), len(regions), len(places))


def extend_bbox(d, lat, lon):
    if 'bbox' not in d:
        d['bbox'] = [[lat, lon], [lat, lon]]
    else:
        d['bbox'][0][0] = min(d['bbox'][0][0], lat)
        d['bbox'][0][1] = min(d['bbox'][0][1], lon)
        d['bbox'][1][0] = max(d['bbox'][1][0], lat)
        d['bbox'][1][1] = max(d['bbox'][1][1], lon)

logging.info('Calculating bboxes')
for k in places:
    if 'lat' in places[k]:
        if 'region' in places[k] and places[k]['region'] in regions:
            extend_bbox(regions[places[k]['region']], places[k]['lat'], places[k]['lon'])
        if 'country' in places[k] and places[k]['country'] in countries:
            extend_bbox(countries[places[k]['country']], places[k]['lat'], places[k]['lon'])

logging.info('Writing')
with open('countries.json', 'w') as f:
    json.dump(countries, f, ensure_ascii=False, indent=1)
with open('regions.json', 'w') as f:
    json.dump(regions, f, ensure_ascii=False, indent=1)
with open('places.json', 'w') as f:
    json.dump(places, f, ensure_ascii=False, indent=1)
logging.info('Done')
