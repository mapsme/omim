# maps_generator
It's tool for generation maps for maps.me application.

## Setup
You must have Python version not lower than 3.6 and complete the following steps:

0. Change directory:
```sh
$ cd omim/tools/python/maps_generator
```
1. [Install generator_tool.](https://github.com/mapsme/omim/blob/master/docs/INSTALL.md)
2. Install dependencies:
```sh
maps_generator$ pip3 install -r requirements.txt
```

3. Make ini file:
```sh
maps_generator$ cp var/etc/map_generator.ini.default var/etc/map_generator.ini
```

4. Edit ini file:
```sh
maps_generator$ vim var/etc/map_generator.ini
```

```ini
[Main]
# The path where the planet will be downloaded and the maps are generated.
MAIN_OUT_PATH: ~/maps_build
# If the flag DEBUG is set a special small planet file will be downloaded.
DEBUG: 1


[Developer]
# The path where the generator_tool will be searched.
BUILD_PATH: ~/omim-build-release
# The path to the project directory omim.
OMIM_PATH: ~/omim


[Generator tool]
# The path to the omim/data.
USER_RESOURCE_PATH: ${Developer:OMIM_PATH}/data
# Do not change it. This is determined automatically.
# NODE_STORAGE: map


[Osm tools]
# The path to the osmctools sources.
OSM_TOOLS_SRC_PATH: ${Developer:OMIM_PATH}/tools/osmctools
# The path where osmctools will be searched or will be built.
OSM_TOOLS_PATH: ~/osmctools


[Logging]
# The path where maps_generator log will be saved.
# LOG_FILE_PATH: generation.log


[External]
# The url to the planet file.
# PLANET_URL:
# The url to the file with md5 sum of the planet.
# PLANET_MD5_URL:
# The base url to WorldCoasts.geom and WorldCoasts.rawgeom (without file name).
# Files latest_coasts.geom and latest_coasts.rawgeom must be at this URL.
# For example, if PLANET_COASTS_URL = https://somesite.com/download/
# The https://somesite.com/download/latest_coasts.geom url will be used to download latest_coasts.geom and
# the https://somesite.com/download/latest_coasts.rawgeom url will be used to download latest_coasts.rawgeom.
# PLANET_COASTS_URL:
# The url to the subway file.
# SUBWAY_URL:

# Urls for production maps generation.
# UGC_URL:
# HOTELS_URL:
# PROMO_CATALOG_CITIES:
# POPULARITY_URL:
# FOOD_URL:
# FOOD_TRANSLATIONS_URL:
# SRTM_PATH:

[Stats]
# Path to rules for calculating statistics by type
STATS_TYPES_CONFIG: ${Developer:OMIM_PATH}/tools/python/maps_generator/var/etc/stats_types_config.txt
```


##### Note: In each field where you need to specify a URL, you can specify the path to the file system using file:///path/to/file

## Usage
```sh
$ cd omim/tools/python
python$ python3.6 -m maps_generator -h
```

```
usage: __main__.py [-h] [--config CONFIG] [-c [CONTINUE]]
                   [--countries COUNTRIES] [--skip SKIP]
                   [--from_stage FROM_STAGE] [--production]

Tool for generation maps for maps.me application.

optional arguments:
  -h, --help            show this help message and exit
  --config CONFIG       Path to config
  -c [CONTINUE], --continue [CONTINUE]
                        Continue the last build or specified in CONTINUE from
                        the last stopped stage.
  --countries COUNTRIES
                        List of regions, separated by a comma or a semicolon,
                        or path to file with regions, separated by a line
                        break, for which maps will be built. The names of the
                        regions can be seen in omim/data/borders. It is
                        necessary to set names without any extension.
  --skip SKIP           List of stages, separated by a comma or a semicolon,
                        for which building will be skipped. Available skip
                        stages: download_external,
                        download_production_external,
                        download_and_convert_planet, update_planet, coastline,
                        preprocess, features, mwm, descriptions,
                        countries_txt, external_resources, localads,
                        statistics, cleanup, index, ugc, popularity, routing,
                        routing_transit.
  --from_stage FROM_STAGE
                        Stage from which maps will be rebuild. Available
                        stages: download_external,
                        download_production_external,
                        download_and_convert_planet, update_planet, coastline,
                        preprocess, features, mwm, descriptions,
                        countries_txt, external_resources, localads,
                        statistics, cleanup, index, ugc, popularity, routing,
                        routing_transit.
  --coasts              Build WorldCoasts.raw and WorldCoasts.rawgeom files
  --production          Build production maps. In another case, 'osm only
                        maps' are built - maps without additional data and
                        advertising.
```

If you are not from the maps.me team, then you do not need the option --production when generating maps.

To generate maps for the whole world you need 400 GB of hard disk space and a computer with more than 64 GB.
### Examples
####  Non-standard planet
If you want to generate maps for Japan you must complete the following steps:
1. Open https://download.geofabrik.de/asia/japan.html and copy url of osm.pbf and md5sum files.
2. Edit ini file:
```sh
maps_generator$ vim var/etc/map_generator.ini
```

```ini
[Main]
...
DEBUG: 0
...
[External]
PLANET_URL: https://download.geofabrik.de/asia/japan-latest.osm.pbf
PLANET_MD5_URL: https://download.geofabrik.de/asia/japan-latest.osm.pbf.md5
...
```

3. Run
```sh
python$ python3.6 -m maps_generator --countries="World, WorldCoasts, Japan_*" --skip="update_planet"

```
You must skip the step of updating the planet, because it is a non-standard planet.
####  Rebuild stages:
For example, you changed routing code in omim project and want to regenerate maps.
You must have previous generation. You may regenerate from stage routing only for two mwms:

```sh
python$ python3.6 -m maps_generator -c --from_stage="routing" --countries="Japan_Kinki Region_Osaka_Osaka, Japan_Chugoku Region_Tottori"

```