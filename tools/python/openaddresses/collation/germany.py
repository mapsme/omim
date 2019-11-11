import itertools
import re

import Levenshtein

from collation.base_address_collators import AddressCollatorForMatching
from collation.address_matching import get_street_variants


class GermanyAddressCollator(AddressCollatorForMatching):
    """Class for comparing OA and OSM addresses in Germany."""

    @classmethod
    def __are_streets_similar(cls, street1, street2):
        """This function recognize street1 and street2 as the same
        if postal lib provides a common variant;
        also deviation in 1 symbol is allowed."""
        variants1 = get_street_variants(street1)
        variants2 = get_street_variants(street2)
        if variants1 & variants2:
            return True
        for variant1, variant2 in itertools.product(variants1, variants2):
            if Levenshtein.distance(variant1, variant2) <= 1:
                return True
        return False

    # Regular expression for pattern "Name one (Name two)"
    STREET_WITH_PLACE_RE = re.compile(r'([^\(]+)\(([^\)]+)\)\s*$')

    @classmethod
    def _do_streets_match(cls, OA_street, OSM_street):
        """Overridden hook method.
        Besides using 'postal' lib this function also treats these cases:
           - "Place name (Street name)" or "Street name (Place name) in OA
             street must match "Street name" in OSM;
           - "Van't-Hoff-Strasse" must match "Van t-Hoff-Strasse",
             or "Johaness-Kraaz-Strasse" must match "Johaness-Kraatz-Strasse"
             (allow Levenshtein distance of 1).
        """
        OA_streets = (OA_street,)
        match_result = cls.STREET_WITH_PLACE_RE.match(OA_street)
        if match_result:
            OA_streets = match_result.groups()
        return any(
            cls.__are_streets_similar(OA_street_, OSM_street)
            for OA_street_ in OA_streets
        )
