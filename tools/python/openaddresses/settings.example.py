# Research mode uses many matching possible outcomes and generates
# a bunch of auxiliary files. Matching mode creates one file with
# roughly "match/not match" verdict for each address.
MODE = 'research'
assert (MODE in ['research', 'matching'])

# Processes in multiprocessing.Pool
WORKERS_CNT = 32

# Limit addresses count to process. Use 10**9 or so as infinity
ADDRESSES_LIMIT = 10**9

# How many records to store in diagnostic files in research mode.
# Use 10**9 or so as infinity but beware of disk storage overflow.
STORED_RECORDS_LIMIT = 100

# Template string for geocoder url, with {lat} and {lon} placeholders
GEOCODER_URL_TEMPLATE = 'https://geocode.maps.me/v1/address?q={lat},{lon}&api_key=__YOUR_KEY__&fallback=false&w=1000&fields=*&query_status=True&limit=100'

# Absolute/relative path base part (excluding extensions) to save
# results to. E.g. '/tmp/berlin-addr/berlin' out-prefix tells to save
# results to '/tmp/berlin-addr/berlin.summary',
# '/tmp/berlin-addr/berlin.ambiguous', etc.
OUT_PREFIX = '/tmp/berlin-addr/berlin'
import os
assert (os.path.isdir(os.path.dirname(OUT_PREFIX)) and
        os.path.basename(OUT_PREFIX) != '')

# Path, possibly glob pattern, to CSV files with OA addresses
# E.g. '/data/openaddr-collected-global/de/**/*.csv'
CSV_SOURCES = '/data/openaddr-collected-global/de/berlin.csv'
