#!/usr/bin/env python3
import sys
import os
import urllib.parse
import urllib.request
import json
import time
from collections import defaultdict
from functools import reduce

with open('places.json', 'r') as f:
    places = json.load(f)

if os.path.exists('places_old.json'):
    with open('places_old.json', 'r') as f:
        places_old = json.load(f)
    for place, names in places_old.items():
        if place in places:
            for k, v in names.items():
                if k not in places[place]:
                    if k.startswith('name') or k.startswith('wiki'):
                        places[place][k] = v

LANGUAGES = ('en cs sk de es fr it ja ko nl ru uk zh pl pt hu ' +
             'th ar da tr sv he id vi ro nb fi el').split(' ')
# LANGUAGES = 'RU EN KA AZ DE HY KK KY MO RO TG TR UZ UK'.split(' ')
WIKIDATA_BASE = ('https://www.wikidata.org/w/api.php?action=wbgetentities&' +
                 'props=labels&format=json&languages={langs}&ids={ids}')
WIKIPEDIA_BASE = ('https://{lang}.wikipedia.org/w/api.php?action=query&format=json&' +
                  'prop=pageprops&ppprop=wikibase_item&titles={titles}')
WIKIPEDIA_API_QUERY_PACK = 10
WIKIDATA_API_QUERY_PACK = 40

# Dict of language -> list((place, article))
from_wikipedia = defaultdict(list)
from_wikidata = []
today = int(time.time())
long_ago = today - 60*60*24*182
for place, names in places.items():
    if names['place'] not in ('city', 'town'):
        continue
    if 'wikidata' in names:
        if names.get('wikidata_date', 0) < long_ago:
            from_wikidata.append((place, names['wikidata']))
    elif 'wikipedia' in names:
        title = names['wikipedia']
        p = title.find(':')
        if p > 0 and p < len(title) - 1:
            from_wikipedia[title[:p]].append((place, title[p+1:]))

sys.stderr.write('Pending {0} articles in {1} languages\n'.format(
    reduce(lambda x, y: x+len(y), from_wikipedia.values(), 0), len(from_wikipedia)))
found = 0
for lang in LANGUAGES:
    if not len(from_wikipedia[lang]):
        continue
    sys.stderr.write('Querying {0} wikipedia articles for lang {1}...'.format(
        len(from_wikipedia[lang]), lang))
    for i in range(0, len(from_wikipedia[lang]), WIKIPEDIA_API_QUERY_PACK):
        sys.stderr.write('{0} '.format(i))
        sys.stderr.flush()
        mapping = {x[1]: x[0] for x in from_wikipedia[lang][i:i+WIKIPEDIA_API_QUERY_PACK]}
        url = WIKIPEDIA_BASE.format(
            lang=lang,
            titles=urllib.parse.quote_plus(u'|'.join(mapping.keys())))
        try:
            time.sleep(1)
            response = urllib.request.urlopen(url)
        except OSError as e:
            print('Error when quering {0}'.format(url))
            raise e
        if response.getcode() == 200:
            wikidata = json.load(response)
            if 'query' in wikidata and 'pages' in wikidata['query']:
                norm = {}
                if 'normalized' in wikidata['query']:
                    for n in wikidata['query']['normalized']:
                        norm[n['to']] = n['from']
                for _, values in wikidata['query']['pages'].items():
                    if 'title' in values and 'pageprops' in values:
                        wp = norm[values['title']] if values['title'] in norm else values['title']
                        if wp in mapping:
                            places[mapping[wp]]['wikidata'] = values['pageprops']['wikibase_item']
                            from_wikidata.append(
                                (mapping[wp], values['pageprops']['wikibase_item']))
                            found += 1
            else:
                print("FAIL at {0}".format(url))
    sys.stderr.write('Done, found {0} wikidata ids\n'.format(found))
sys.stderr.flush()

with open('places.json', 'w') as f:
    json.dump(places, f, ensure_ascii=False, indent=1)

sys.stderr.write('Querying {0} wikidata entities...'.format(len(from_wikidata)))
for i in range(0, len(from_wikidata), WIKIDATA_API_QUERY_PACK):
    sys.stderr.write('{0} '.format(i))
    sys.stderr.flush()
    mapping = {x[1]: x[0] for x in from_wikidata[i:i+WIKIDATA_API_QUERY_PACK]}
    url = WIKIDATA_BASE.format(langs='|'.join(LANGUAGES), ids='|'.join(mapping.keys()))
    time.sleep(1)
    response = urllib.request.urlopen(url)
    if response.getcode() != 200:
        continue
    wikidata = json.load(response)
    if 'entities' in wikidata:
        for entity, labels in wikidata['entities'].items():
            if entity in mapping and 'labels' in labels:
                places[mapping[entity]]['wikidata_date'] = today
                for lang in labels['labels']:
                    if lang not in mapping[entity]:
                        if 'name_' + lang not in places[mapping[entity]]:
                            places[mapping[entity]]['name_' + lang] = labels['labels'][lang]['value']
sys.stderr.write('Done\n')
sys.stderr.flush()

with open('places.json', 'w') as f:
    json.dump(places, f, ensure_ascii=False, indent=1)
