from collation.address_matching import (
    do_housenumbers_match,
    is_housenumber_in_range,
    do_streets_match_postal,
    do_streets_match_weak
)


class AddressCollateResult():
    """Stores address collation result zipped into one variable. Access to
    different components is provided through the class properties
    'street_match', 'street_match_weak', 'housenumber_match',
    'housenumber_in_range'."""

    STREET_MATCH_BIT = 1 << 0
    STREET_MATCH_WEAK_BIT = 1 << 1
    HOUSENUMBER_MATCH_BIT = 1 << 2
    HOUSENUMBER_IN_RANGE_BIT = 1 << 3

    KEYS = (
        'street_match',
        'street_match_weak',
        'housenumber_match',
        'housenumber_in_range'
    )

    def __init__(self, **kwargs):
        self.bitmask = 0
        for key in self.KEYS:
            if key in kwargs:
                setattr(self, key, kwargs[key])

    def __repr__(self):
        kwargs = ','.join(f"{key}={getattr(self, key)}" for key in self.KEYS)
        return f"AddressCollateResult({kwargs})"

    @property
    def street_match(self):
        return bool(self.bitmask & self.STREET_MATCH_BIT)

    @street_match.setter
    def street_match(self, is_street_match):
        if is_street_match:
            self.bitmask |= self.STREET_MATCH_BIT
        else:
            self.bitmask &= ~self.STREET_MATCH_BIT

    @property
    def street_match_weak(self):
        return bool(self.bitmask & self.STREET_MATCH_WEAK_BIT)

    @street_match_weak.setter
    def street_match_weak(self, is_street_match_weak):
        if is_street_match_weak:
            self.bitmask |= self.STREET_MATCH_WEAK_BIT
        else:
            self.bitmask &= ~self.STREET_MATCH_WEAK_BIT

    @property
    def housenumber_match(self):
        return bool(self.bitmask & self.HOUSENUMBER_MATCH_BIT)

    @housenumber_match.setter
    def housenumber_match(self, is_housenumber_match):
        if is_housenumber_match:
            self.bitmask |= self.HOUSENUMBER_MATCH_BIT
        else:
            self.bitmask &= ~self.HOUSENUMBER_MATCH_BIT

    @property
    def housenumber_in_range(self):
        return bool(self.bitmask & self.HOUSENUMBER_IN_RANGE_BIT)

    @housenumber_in_range.setter
    def housenumber_in_range(self, is_housenumber_in_range):
        if is_housenumber_in_range:
            self.bitmask |= self.HOUSENUMBER_IN_RANGE_BIT
        else:
            self.bitmask &= ~self.HOUSENUMBER_IN_RANGE_BIT


class AddressCollatorForResearch:
    """This class compares addresses for analysis. It interprets 'street_match'
    quite strictly (no misprints or other variances), but also returns
    'street_match_weak' flag."""

    @classmethod
    def collate(cls, OA_street, OA_hn, OSM_street, OSM_hn):
        street_match = do_streets_match_postal(OA_street, OSM_street)
        street_match_weak = (street_match or
                                do_streets_match_weak(OA_street, OSM_street))
        housenumber_match = do_housenumbers_match(OA_hn, OSM_hn)
        housenumber_in_range = (housenumber_match or
                                is_housenumber_in_range(OA_hn, OSM_hn))
        return AddressCollateResult(
            street_match=street_match,
            street_match_weak=street_match_weak,
            housenumber_match=housenumber_match,
            housenumber_in_range=housenumber_in_range
        )


class AddressCollatorForMatching:
    """Base class for matching addresses in a specific region."""

    @classmethod
    def collate(cls, OA_street, OA_hn, OSM_street, OSM_hn):
        """Template method"""
        street_match = cls._do_streets_match(OA_street, OSM_street)
        housenumber_match = cls._do_housenumbers_match(OA_hn, OSM_hn)
        housenumber_in_range = (housenumber_match or
                                cls._is_housenumber_in_range(OA_hn, OSM_hn))
        return AddressCollateResult(
            street_match=street_match,
            housenumber_match=housenumber_match,
            housenumber_in_range=housenumber_in_range
        )

    @classmethod
    def _do_streets_match(cls, OA_street, OSM_street):
        """Hook method"""
        return do_streets_match_postal(OA_street, OSM_street)

    @classmethod
    def _do_housenumbers_match(cls, OA_hn, OSM_hn):
        """Hook method"""
        return do_housenumbers_match(OA_hn, OSM_hn)

    @classmethod
    def _is_housenumber_in_range(cls, OA_hn, OSM_hn):
        """Hook method"""
        return is_housenumber_in_range(OA_hn, OSM_hn)
