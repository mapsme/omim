#!/bin/bash
# This script prepares countries/regions/places feeds for trinet and locals.
set -e -u

# Backup places.json to fetch translations from it later.
[ -f places.json ] && mv places.json places_old.json

# Create bare feeds from the database.
./places_feed.py

# Query wikidata for missing places translations.
./translate_places.py

# Remove the backup file.
[ -f places_old.json ] && rm places_old.json

# Convert json to csv.
for i in {places,regions,countries}.json; do
  echo "$i"
  ./flat_json_to_csv.py "$i" seo > "$(basename "$i" .json)_seo.csv"
done

# Archive
tar -czvf countries_regions_places.tgz *_seo.csv

# Upload to transfer.sh and copy the link to the clipboard
curl --upload-file countries_regions_places.tgz https://transfer.sh/countries_regions_places.tgz | tee /dev/tty | pbcopy
