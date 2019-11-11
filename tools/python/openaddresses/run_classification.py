import csv
import datetime
import functools
import itertools
import json
import logging
import multiprocessing
import pprint
import requests

from collections import Counter
from requests.packages.urllib3.exceptions import InsecureRequestWarning

from address import Address
from classification import (
    AddressClassifierForMatching,
    AddressClassifierForResearch
)
from collation.base_address_collators import (
    AddressCollatorForResearch
)
from input_adapters import OpenAddressesMultipleCSVsInputAdapter


requests.packages.urllib3.disable_warnings(InsecureRequestWarning)

from settings import *


def get_weight1_addresses(geocoder_addresses):
    return [tuple(x['address_details'][part]
                  for part in
                  ('locality', 'sublocality', 'suburb', 'street', 'building')
                  if x['address_details'].get(part)
            ) for x in geocoder_addresses if x['weight'] == 1.0]


def shoot_address(oa_address, classifier):
    """Send an address to geocoder and get matching results.
    'oa_address' - OpenAddress address in the form of Address namedtuple.
    'classifier' - an object to classify the address.
    """
    no_address = not oa_address.housenumber or not oa_address.street
    if no_address:
        return 'empty_address', None, oa_address

    url = GEOCODER_URL_TEMPLATE.format(lat=oa_address.lat, lon=oa_address.lon)

    shot_result = None
    for i in range(10):  # number of tries to get geocoder response
        addresses = None
        try:
            response = requests.get(url, verify=False)
            geocoder_response = response.json()
            status = geocoder_response['status']['nominator']['code']
        except Exception as e:
            continue

        if status == 200:
            geocoder_addresses = [
                x for x in geocoder_response['results']
                if all(key in x['address_details']
                       for key in ('locality', 'street', 'building'))
            ]
            shot_result = classifier.classify(oa_address, geocoder_addresses)
            addresses = get_weight1_addresses(geocoder_addresses)  # to analyse why match_not/nothing
        elif status == 404:
            shot_result = 'miss'

        if shot_result:
            break

    if not shot_result:
        shot_result = 'failed'
    return shot_result, addresses, oa_address


SHOT_COMMON_OUTCOMES = (
    'empty_address',  # OA line doesn't contain a valid address.
    'miss',           # Geocoder returned 404.
    'failed'          # Geocoder has not ended up with 200/404 code in 10 tries.
)


def do_research(addresses_iterable, out_prefix, classifier):
    """Bombard geocoder with addresses from addresses_iterable and accumulate
    different stats.
    """

    oa_streets = set()  # Store all street names, to dump into a file
    osm_streets = set()  # Store all street names, to dump into a file
    miss_list = []  # List of oa_addresses for with geocoder returns 404
    match_near_list = []  # List of oa_addresses with 'match_near' outcome
    street_mismatch_list = []  # List of (oa_address, gc_addresses)
    street_similar_list = []  # List of (oa_address, gc_addresses)
    match_not_list = []  # List of (oa_address, gc_addresses)
    match_nothing_list = []  # List of oa_addresses
    ambiguous_list = []  # List of oa_addresses for which geocoder returns more
                         # than one weight=1 results
    addresses = dict()  # Strings that geocoder returned (like "USA, New York,
                        # North Avenue, 2342)" => [list of OA addresses]

    summary = Counter({
        key: 0 for key in
            itertools.chain(classifier.OUTCOMES,
                            SHOT_COMMON_OUTCOMES,
                            ('ambiguous',))
    })

    worker = functools.partial(shoot_address, classifier=classifier)

    limited_address_iterable = (
        oa_address for oa_address, i
        in zip(addresses_iterable, range(ADDRESSES_LIMIT))
    )

    with multiprocessing.Pool(processes=WORKERS_CNT) as pool:
        begin_datetime = datetime.datetime.now()
        results = pool.imap_unordered(worker, limited_address_iterable, chunksize=20)
        for outcome, gc_addresses, oa_address in results:
            summary[outcome] += 1
            summary['records_cnt'] += 1

            if outcome == 'miss':
                if len(miss_list) < STORED_RECORDS_LIMIT:
                    miss_list.append(oa_address)
            elif outcome == 'match_near':
                if len(match_near_list) < STORED_RECORDS_LIMIT:
                    match_near_list.append(oa_address)
            elif outcome == 'street_mismatch':
                if len(street_mismatch_list) < STORED_RECORDS_LIMIT:
                    street_mismatch_list.append((oa_address, gc_addresses))
            elif outcome == 'street_similar':
                if len(street_similar_list) < STORED_RECORDS_LIMIT:
                    street_similar_list.append((oa_address, gc_addresses))
            elif outcome == 'match_not':
                if len(match_not_list) < STORED_RECORDS_LIMIT:
                    match_not_list.append(oa_address)
            elif outcome == 'match_nothing':
                if len(match_nothing_list) < STORED_RECORDS_LIMIT:
                    match_nothing_list.append((oa_address, gc_addresses))

            if gc_addresses:
                for addr in gc_addresses:
                    addresses.setdefault(addr, []).append(oa_address)
                if (len(gc_addresses) > 1 and
                        len(ambiguous_list) < STORED_RECORDS_LIMIT):
                    ambiguous_list.append(oa_address)
                    summary['ambiguous'] += 1
                osm_streets.update(x[-2] for x in gc_addresses)
            oa_streets.add(oa_address.street)

        end_datetime = datetime.datetime.now()

    summary['time'] = str(end_datetime - begin_datetime)
    multiple_hits = {k: v for k, v in addresses.items() if len(v) > 1}

    with open(out_prefix + '.summary', 'w') as f:
        json.dump(dict(summary), f, ensure_ascii=False, indent=2)
    with open(out_prefix + '.miss', 'w') as f:
        json.dump(miss_list, f, ensure_ascii=False, indent=2)
    with open(out_prefix + '.match_near', 'w') as f:
        json.dump(match_near_list, f, ensure_ascii=False, indent=2)
    with open(out_prefix + '.multiple', 'w') as f:
        json.dump(list(multiple_hits)[:STORED_RECORDS_LIMIT],
                  f, ensure_ascii=False, indent=2)
    with open(out_prefix + '.ambiguous', 'w') as f:
        json.dump(ambiguous_list, f, ensure_ascii=False, indent=2)
    with open(out_prefix + '.street_mismatch', 'w') as f:
        json.dump(street_mismatch_list, f, ensure_ascii=False, indent=2)
    with open(out_prefix + '.oa_streets', 'w') as f:
        json.dump(sorted(oa_streets), f, ensure_ascii=False, indent=2)
    with open(out_prefix + '.osm_streets', 'w') as f:
        json.dump(sorted(osm_streets), f, ensure_ascii=False, indent=2)
    with open(out_prefix + '.street_similar', 'w') as f:
        json.dump(street_similar_list, f, ensure_ascii=False, indent=2)
    with open(out_prefix + '.match_not', 'w') as f:
        json.dump(match_not_list, f, ensure_ascii=False, indent=2)
    with open(out_prefix + '.match_nothing', 'w') as f:
        json.dump(match_nothing_list, f, ensure_ascii=False, indent=2)
    logging.info(pprint.pformat(dict(summary)))


def do_matching(addresses_iterable, out_prefix, classifier):
    """Bombard geocoder with addresses from addresses_iterable and classify
    addresses in accordance with matching outcome.
    """
    summary = Counter({
        key: 0 for key in
        itertools.chain(classifier.OUTCOMES,
                        SHOT_COMMON_OUTCOMES)
    })

    worker = functools.partial(shoot_address, classifier=classifier)

    limited_address_iterable = (
        oa_address for oa_address, i
        in zip(addresses_iterable, range(ADDRESSES_LIMIT))
    )

    with open(out_prefix + '.classify.csv', 'w', newline='') as csvfile:
        writer = csv.writer(csvfile, dialect='excel')
        writer.writerow(Address._fields + ('outcome',))
        with multiprocessing.Pool(processes=WORKERS_CNT) as pool:
            begin_datetime = datetime.datetime.now()
            results = pool.imap_unordered(worker, limited_address_iterable,
                                          chunksize=20)
            for outcome, gc_addresses, oa_address in results:
                summary[outcome] += 1
                summary['records_cnt'] += 1
                writer.writerow(oa_address + (outcome,))

            end_datetime = datetime.datetime.now()
            summary['time'] = str(end_datetime - begin_datetime)

            with open(out_prefix + '.summary', 'w') as f:
                json.dump({k: v for k, v in summary.items()},
                          f, ensure_ascii=False, indent=2)


def main():
    logging.getLogger().setLevel(logging.DEBUG)

    try:
        t1 = datetime.datetime.now()
        oa_addresses = OpenAddressesMultipleCSVsInputAdapter(CSV_SOURCES)
        if MODE == 'research':
            classifier = AddressClassifierForResearch(
                collator_class=AddressCollatorForResearch
            )
            do_research(oa_addresses, OUT_PREFIX, classifier)
        else:
            from collation.germany import GermanyAddressCollator
            classifier = AddressClassifierForMatching(
                collator_class=GermanyAddressCollator
            )
            do_matching(oa_addresses, OUT_PREFIX, classifier)
    except BaseException as e:
        logging.error('Main process failed:', exc_info=1)
    finally:
        t2 = datetime.datetime.now()
        logging.info(f'Execution time = {t2 - t1}')


if __name__ == '__main__':
    main()
