This tool allows to perform OpenAddresses and OSM addresses matching, 
in generously-logging 'research' mode or in nearly binary-answering 
'matching' mode. OpenAddresses files are available at
http://results.openaddresses.io in already compiled format. OSM data are
employed via Maps.Me b2b geocoder. Write and use your own location-specific
classifier classes for different countries/locations for better
housenumber/street matching (see collation/germany.py for an example).

The main script is "run_classification.py".

Create "settings.py" from "settings.example.py" and tune parameters there.