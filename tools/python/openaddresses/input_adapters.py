import csv
import glob

from address import Address


# OpenAddresses csv files has the following columns:
# LON,LAT,NUMBER,STREET,UNIT,CITY,DISTRICT,REGION,POSTCODE,ID,HASH
OA_LON_IDX = 0
OA_LAT_IDX = 1
OA_HOUSENUMBER_IDX = 2
OA_STREET_IDX = 3


def OpenAddressesCSVInputAdapter(csv_path):
    """Take path to a CSV file with addresses in OpenAddresses format.
    Generates Address tuples."""
    with open(csv_path) as csv_file:
        reader = csv.reader(csv_file)
        next(reader)  # skip header
        for row in reader:
            yield Address(
                lat=row[OA_LAT_IDX],
                lon=row[OA_LON_IDX],
                street=row[OA_STREET_IDX],
                housenumber=row[OA_HOUSENUMBER_IDX]
            )


def OpenAddressesMultipleCSVsInputAdapter(csv_file_mask, recursive=True):
    """As opposed to OpenAddressesCSVInputAdapter, this generator takes a
    file mask which may contain glob patterns. E.g., file mask /tmp/de/**/*.csv
    with recursive=True matches
    /tmp/de/berlin.csv
    as well as
    /tmp/de/hh/statewide.csv
    """

    for csv_file in glob.glob(csv_file_mask, recursive=recursive):
        address_generator = OpenAddressesCSVInputAdapter(csv_file)
        for address in address_generator:
            yield address
