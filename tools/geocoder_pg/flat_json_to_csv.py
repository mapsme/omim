#!/usr/bin/env python3
import sys
import json
import csv
from random import randint

if len(sys.argv) < 1:
    print('Usage: {} <json> [seo]'.format(sys.argv[1]))
    sys.exit(1)

with open(sys.argv[1], 'r') as f:
    data = json.load(f)

if len(sys.argv) > 2:
    if 'place' in sys.argv[1]:
        photo = ('https://upload.wikimedia.org/wikipedia/commons/thumb' +
                 '/f/fb/Turkish.town.cesme.jpg/640px-Turkish.town.cesme.jpg')
    elif 'region' in sys.argv[1]:
        photo = ('http://trash-russia.com/wp-content/uploads/2014/04/' +
                 'best-photos-of-russia-2013-part-9-10.jpg')
    else:
        photo = 'http://www.handmaps.org/maps/hand_drawn_map_248.jpg'
    for k in data:
        data[k]['rating'] = randint(1, 5)
        data[k]['photo'] = photo

LANG_LIST = ('en ja fr ar de ru sv zh fi be ka ko he nl ga el it es th cy sr uk ca hu eu ' +
             'fa br pl hy kn sl ro sq am fy cs gd sk af lb pt hr vi tr bg eo lt la kk gsw ' +
             'et ku mn mk lv hi')
LANGUAGES = ['name', 'int_name'] + ['name_'+x for x in LANG_LIST.split()]

columns = set()
names = set()
for cid, values in data.items():
    for d in values:
        if 'name' in d:
            if d in LANGUAGES:
                names.add(d)
        elif d not in ('bbox', 'wikidata_date'):
            columns.add(d)

columns = ['id'] + sorted(columns) + sorted(names)
w = csv.writer(sys.stdout, delimiter=';')
w.writerow(columns)
for d, v in data.items():
    row = [d]
    for c in columns[1:]:
        row.append('' if c not in v else v[c])
    w.writerow(row)
