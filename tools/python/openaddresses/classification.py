class AddressClassifierForResearch:
    """Classifier to investigate OA/OSM address data quality."""

    def __init__(self, collator_class):
        """collator_class defines street/housenumber comparison rules."""
        self.collator_class = collator_class

    OUTCOMES = (
        'match_strict',    # There is rather-strict-matching address at that point (with weight = 1).
        'match_near',      # There is rather-strict-matching address near that point (with weight < 1).
        'match_hn_range',  # There is an address with matching street and with
                           # housenumber in the form of range which contains
                           # oa_address's housenumber.
        'street_similar',  # Housenumber matched, street not matched but look similar (with weak collation).
        'street_mismatch', # Housenumber matched, but street is different.
        'match_not',       # There is no matching address at that point or nearby,
                           # but an address exists at that spot (an address with weight 1.0).
        'match_nothing'    # There is no matching address at that point or nearby,
                           # and no an address at that spot (an address with weight 1.0).
    )

    def classify(self, oa_address, geocoder_addresses):
        """Returns a value from OUTCOMES."""

        if not geocoder_addresses:
            return 'match_nothing'

        collate_results = [
            (
                self.collator_class.collate(
                    oa_address.street,
                    oa_address.housenumber,
                    gc_address['address_details']['street'],
                    gc_address['address_details']['building']
                ),
                gc_address['weight']
            ) for gc_address in geocoder_addresses
        ]

        match_strict = any(
            res.street_match and res.housenumber_match and weight == 1.0
            for res, weight in collate_results
        )
        if match_strict:
            return 'match_strict'

        match_near = any(
            res.street_match and res.housenumber_match # and weight < 1.0
            for res, weight in collate_results
        )
        if match_near:
            return 'match_near'

        match_hn_range = any(
            res.street_match and res.housenumber_in_range
            for res, weight in collate_results
        )
        if match_hn_range:
            return 'match_hn_range'

        # Further we consider only geocoder addresses with weight=1
        collate_results = [
            res for res, weight in collate_results if weight == 1.0
        ]
        if not collate_results:
            return 'match_nothing'

        street_similar = any(
            res.street_match_weak and res.housenumber_in_range
            for res in collate_results
        )
        if street_similar:
            return 'street_similar'

        street_mismatch = any(
            res.housenumber_in_range
            for res in collate_results
        )
        if street_mismatch:
            return 'street_mismatch'

        return 'match_not'


class AddressClassifierForMatching:
    """Classifier for matching OA addresses with OSM."""

    def __init__(self, collator_class):
        """collator_class defines street/housenumber comparison rules."""
        self.collator_class = collator_class

    OUTCOMES = (
        'match_strict',   # There is matching address at that point or nearby.
        'match_hn_range', # There is an address with matching street and with
                          # housenumber in the form of range which contains
                          # oa_address's housenumber.
        'match_not',      # There is no matching address at that point or nearby,
                          # but an address exists at that spot (an address with weight 1.0).
        'match_nothing'   # There is no matching address at that point or nearby,
                          # and no an address at that spot (an address with weight 1.0).
    )

    def classify(self, oa_address, geocoder_addresses):
        """Returns a value from OUTCOMES."""

        if not geocoder_addresses:
            return 'match_nothing'

        collate_results = [
            self.collator_class.collate(
                oa_address.street,
                oa_address.housenumber,
                gc_address['address_details']['street'],
                gc_address['address_details']['building']
            ) for gc_address in geocoder_addresses
        ]

        match_strict = any(res.street_match and res.housenumber_match
                            for res in collate_results)
        if match_strict:
            return 'match_strict'

        match_hn_range = any(res.street_match and res.housenumber_in_range
                                for res in collate_results)
        if match_hn_range:
            return 'match_hn_range'

        address_with_weight_1_returned = any(
            gc_address['weight'] == 1.0 for gc_address in geocoder_addresses
        )
        if address_with_weight_1_returned:
            return 'match_not'

        return 'match_nothing'
