"""Address collation functions common for all locations."""

import itertools
import re
from postal import expand as postal_expand

import Levenshtein


def normalize_housenumber(hn):
    hn = hn.replace(' ', '').lower()
    return hn


def do_housenumbers_match(OA_hn, OSM_hn):
    return normalize_housenumber(OA_hn) == normalize_housenumber(OSM_hn)


def unfold_numbers(number_ranges):
    """numbers_ranges is a string like "10-12;20a,22-23". The function
    returns a list of strings, e.g. ['10','11','12','20a','22','23']."""
    numbers = []
    ranges = itertools.chain.from_iterable(
        x.split(',') for x in number_ranges.split(';')
    )
    for range_ in ranges:
        parts = range_.split('-')
        if len(parts) == 1:
            numbers.append(parts[0])
        elif len(parts) == 2:
            begin, end = parts
            if not (begin.isdigit() and end.isdigit()):
                continue
            begin = int(begin)
            end = int(end)
            if end >= begin:
                numbers.extend(map(str, range(begin, end+1)))
    return numbers


def do_housenumbers_match(OA_hn, OSM_hn):
    return normalize_housenumber(OA_hn) == normalize_housenumber(OSM_hn)


def is_housenumber_in_range(OA_hn, OSM_hn):
    """Is an OA housenumber in OSM_hn taking into account that OSM_hn may be
    a range and/or enumeration of housenumbers."""
    return (normalize_housenumber(OA_hn) in
            map(normalize_housenumber, unfold_numbers(OSM_hn)))


def get_street_variants(street):
    variants = set(
        postal_expand.expand_address(
            street,
            address_components=postal_expand.ADDRESS_STREET
        )
    )
    return variants


def do_streets_match_postal(street1, street2):
    variants1 = get_street_variants(street1)
    variants2 = get_street_variants(street2)
    return not variants1.isdisjoint(variants2)


STREET_SPLIT_RE = re.compile(r' |\.|\(|\)')


def get_longest_word(street):
    words = STREET_SPLIT_RE.split(street)
    longest_word = sorted([x for x in words if len(x) == max(len(y) for y in words)])[0]
    return longest_word


def do_streets_match_weak(street1, street2):
    """Weak street comparison suggests that the longest word in two streets are
    alike, so that each forth letter may be misspelled."""
    longest_word1 = get_longest_word(street1)
    longest_word2 = get_longest_word(street2)
    variants1 = get_street_variants(longest_word1)
    variants2 = get_street_variants(longest_word2)
    max_distance = max(len(longest_word1), len(longest_word2)) // 4
    for variant1, variant2 in itertools.product(variants1, variants2):
        if Levenshtein.distance(variant1, variant2) <= max_distance:
            return True
    return False