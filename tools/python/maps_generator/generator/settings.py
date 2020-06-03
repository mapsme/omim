import argparse
import multiprocessing
import os
import sys
from configparser import ConfigParser
from configparser import ExtendedInterpolation
from pathlib import Path
from typing import Any
from typing import AnyStr

from maps_generator.utils.md5 import md5_ext
from maps_generator.utils.system import total_virtual_memory

parser = argparse.ArgumentParser(add_help=False)
opt_config = "--config"
parser.add_argument(opt_config, type=str, default="", help="Path to config")


def get_config_path(config_path: AnyStr):
    """
    It tries to get an opt_config value.
    If doesn't get the value a function returns config_path.
    """
    argv = sys.argv
    indexes = (-1, -1)
    for i, opt in enumerate(argv):
        if opt.startswith(f"{opt_config}="):
            indexes = (i, i + 1)
        if opt == opt_config:
            indexes = (i, i + 2)

    if indexes[1] > len(argv):
        return config_path

    args = argv[indexes[0] : indexes[1]]
    return parser.parse_args(args).config if args else config_path


class CfgReader:
    """
    Config reader.
    There are 3 way of getting an option. In priority order:
        1. From system env.
        2. From config.
        3. From default values.

    For using the option from system env you can build an option name as
    MM__ + [SECTION_NAME] + _ + [VALUE_NAME].
    """

    def __init__(self, default_settings_path: AnyStr):
        self.config = ConfigParser(interpolation=ExtendedInterpolation())
        self.config.read([get_config_path(default_settings_path)])

    def get_opt(self, s: AnyStr, v: AnyStr, default: Any = None):
        val = CfgReader._get_env_val(s, v)
        if val is not None:
            return val

        return self.config.get(s, v) if self.config.has_option(s, v) else default

    def get_opt_path(self, s: AnyStr, v: AnyStr, default: AnyStr = ""):
        return os.path.expanduser(self.get_opt(s, v, default))

    @staticmethod
    def _get_env_val(s: AnyStr, v: AnyStr):
        return os.environ.get(f"MM__{s.upper()}_{v.upper()}")


DEFAULT_PLANET_URL = "https://planet.openstreetmap.org/pbf/planet-latest.osm.pbf"
DEFAULT_PLANET_MD5_URL = md5_ext(DEFAULT_PLANET_URL)

# Main section:
# If DEBUG is True, a little special planet is downloaded.
DEBUG = True
_HOME_PATH = str(Path.home())
_WORK_PATH = _HOME_PATH
TMPDIR = os.path.join(_HOME_PATH, "tmp")
MAIN_OUT_PATH = os.path.join(_WORK_PATH, "generation")

# Developer section:
BUILD_PATH = os.path.join(_WORK_PATH, "omim-build-release")
OMIM_PATH = os.path.join(_WORK_PATH, "omim")

# Osm tools section:
OSM_TOOLS_SRC_PATH = os.path.join(OMIM_PATH, "tools", "osmctools")
OSM_TOOLS_PATH = os.path.join(_WORK_PATH, "osmctools")

# Generator tool section:
NODE_STORAGE = "mem" if total_virtual_memory() / 10 ** 9 >= 64 else "map"
USER_RESOURCE_PATH = os.path.join(OMIM_PATH, "data")

# Stages section:
NEED_PLANET_UPDATE = False

# Logging section:
LOG_FILE_PATH = os.path.join(MAIN_OUT_PATH, "generation.log")

# External resources section:
PLANET_URL = DEFAULT_PLANET_URL
PLANET_MD5_URL = DEFAULT_PLANET_MD5_URL
PLANET_COASTS_URL = ""
UGC_URL = ""
HOTELS_URL = ""
PROMO_CATALOG_CITIES_URL = ""
PROMO_CATALOG_COUNTRIES_URL = ""
POPULARITY_URL = ""
SUBWAY_URL = ""
TRANSIT_URL = ""
FOOD_URL = ""
FOOD_TRANSLATIONS_URL = ""
UK_POSTCODES_URL = ""
US_POSTCODES_URL = ""
SRTM_PATH = ""
ISOLINES_PATH = ""

# Stats section:
STATS_TYPES_CONFIG = ""

# Other variables:
PLANET = "planet"
GEN_TOOL = "generator_tool"
VERSION_FILE_NAME = "version.txt"

# Osm tools:
OSM_TOOL_CONVERT = "osmconvert"
OSM_TOOL_FILTER = "osmfilter"
OSM_TOOL_UPDATE = "osmupdate"
OSM_TOOLS_CC = "cc"
OSM_TOOLS_CC_FLAGS = [
    "-O3",
]

# Planet and coasts:
PLANET_COASTS_GEOM_URL = os.path.join(PLANET_COASTS_URL, "latest_coasts.geom")
PLANET_COASTS_RAWGEOM_URL = os.path.join(PLANET_COASTS_URL, "latest_coasts.rawgeom")

if DEBUG:
    PLANET_URL = "https://www.dropbox.com/s/m3ru5tnj8g9u4cz/planet-latest.o5m?raw=1"
    PLANET_MD5_URL = (
        "https://www.dropbox.com/s/8wdl2hy22jgisk5/planet-latest.o5m.md5?raw=1"
    )
    NODE_STORAGE = "map"
    NEED_PLANET_UPDATE = False

# Common:
THREADS_COUNT = multiprocessing.cpu_count()

# for lib logging
LOGGING = {
    "version": 1,
    "disable_existing_loggers": False,
    "formatters": {
        "standard": {"format": "[%(asctime)s] %(levelname)s %(module)s %(message)s"},
    },
    "handlers": {
        "stdout": {
            "level": "INFO",
            "class": "logging.StreamHandler",
            "formatter": "standard",
        },
        "file": {
            "level": "DEBUG",
            "class": "logging.handlers.WatchedFileHandler",
            "formatter": "standard",
            "filename": LOG_FILE_PATH,
        },
    },
    "loggers": {
        "maps_generator": {
            "handlers": ["stdout", "file"],
            "level": "DEBUG",
            "propagate": True,
        }
    },
}


def init(default_settings_path: AnyStr):
    # Try to read a config and to overload default settings
    cfg = CfgReader(default_settings_path)

    # Main section:
    global DEBUG
    global MAIN_OUT_PATH
    global TMPDIR
    _DEBUG = cfg.get_opt("Main", "DEBUG")
    DEBUG = DEBUG if _DEBUG is None else int(_DEBUG)
    MAIN_OUT_PATH = cfg.get_opt_path("Main", "MAIN_OUT_PATH", MAIN_OUT_PATH)
    TMPDIR = cfg.get_opt_path("Main", "TMPDIR", TMPDIR)

    # Developer section:
    global BUILD_PATH
    global OMIM_PATH
    BUILD_PATH = cfg.get_opt_path("Developer", "BUILD_PATH", BUILD_PATH)
    OMIM_PATH = cfg.get_opt_path("Developer", "OMIM_PATH", OMIM_PATH)

    # Osm tools section:
    global OSM_TOOLS_SRC_PATH
    global OSM_TOOLS_PATH
    OSM_TOOLS_SRC_PATH = cfg.get_opt_path(
        "Osm tools", "OSM_TOOLS_SRC_PATH", OSM_TOOLS_SRC_PATH
    )
    OSM_TOOLS_PATH = cfg.get_opt_path("Osm tools", "OSM_TOOLS_PATH", OSM_TOOLS_PATH)

    # Generator tool section:
    global USER_RESOURCE_PATH
    global NODE_STORAGE
    USER_RESOURCE_PATH = cfg.get_opt_path(
        "Generator tool", "USER_RESOURCE_PATH", USER_RESOURCE_PATH
    )
    NODE_STORAGE = cfg.get_opt("Generator tool", "NODE_STORAGE", NODE_STORAGE)

    # Stages section:
    global NEED_PLANET_UPDATE
    NEED_PLANET_UPDATE = cfg.get_opt("Stages", "NEED_PLANET_UPDATE", NEED_PLANET_UPDATE)

    # Logging section:
    global LOG_FILE_PATH
    global LOGGING
    LOG_FILE_PATH = os.path.join(MAIN_OUT_PATH, "generation.log")
    LOG_FILE_PATH = cfg.get_opt_path("Logging", "MAIN_LOG", LOG_FILE_PATH)
    os.makedirs(os.path.dirname(os.path.abspath(LOG_FILE_PATH)), exist_ok=True)
    LOGGING["handlers"]["file"]["filename"] = LOG_FILE_PATH

    # External sction:
    global PLANET_URL
    global PLANET_MD5_URL
    global PLANET_COASTS_URL
    global UGC_URL
    global HOTELS_URL
    global PROMO_CATALOG_CITIES_URL
    global PROMO_CATALOG_COUNTRIES_URL
    global POPULARITY_URL
    global SUBWAY_URL
    global TRANSIT_URL
    global FOOD_URL
    global UK_POSTCODES_URL
    global US_POSTCODES_URL
    global FOOD_TRANSLATIONS_URL
    global SRTM_PATH
    global ISOLINES_PATH

    PLANET_URL = cfg.get_opt_path("External", "PLANET_URL", PLANET_URL)
    PLANET_MD5_URL = cfg.get_opt_path("External", "PLANET_MD5_URL", PLANET_MD5_URL)
    PLANET_COASTS_URL = cfg.get_opt_path(
        "External", "PLANET_COASTS_URL", PLANET_COASTS_URL
    )
    UGC_URL = cfg.get_opt_path("External", "UGC_URL", UGC_URL)
    HOTELS_URL = cfg.get_opt_path("External", "HOTELS_URL", HOTELS_URL)
    PROMO_CATALOG_CITIES_URL = cfg.get_opt_path(
        "External", "PROMO_CATALOG_CITIES_URL", PROMO_CATALOG_CITIES_URL
    )
    PROMO_CATALOG_COUNTRIES_URL = cfg.get_opt_path(
        "External", "PROMO_CATALOG_COUNTRIES_URL", PROMO_CATALOG_COUNTRIES_URL
    )
    POPULARITY_URL = cfg.get_opt_path("External", "POPULARITY_URL", POPULARITY_URL)
    SUBWAY_URL = cfg.get_opt("External", "SUBWAY_URL", SUBWAY_URL)
    TRANSIT_URL = cfg.get_opt("External", "TRANSIT_URL", TRANSIT_URL)
    FOOD_URL = cfg.get_opt("External", "FOOD_URL", FOOD_URL)

    UK_POSTCODES_URL = cfg.get_opt("External", "UK_POSTCODES_URL", UK_POSTCODES_URL)
    US_POSTCODES_URL = cfg.get_opt("External", "US_POSTCODES_URL", US_POSTCODES_URL)
    FOOD_TRANSLATIONS_URL = cfg.get_opt(
        "External", "FOOD_TRANSLATIONS_URL", FOOD_TRANSLATIONS_URL
    )
    SRTM_PATH = cfg.get_opt_path("External", "SRTM_PATH", SRTM_PATH)
    ISOLINES_PATH = cfg.get_opt_path("External", "ISOLINES_PATH", ISOLINES_PATH)

    # Stats section:
    global STATS_TYPES_CONFIG
    STATS_TYPES_CONFIG = cfg.get_opt_path(
        "Stats", "STATS_TYPES_CONFIG", STATS_TYPES_CONFIG
    )

    # Common:
    global THREADS_COUNT
    threads_count = int(cfg.get_opt("Common", "THREADS_COUNT", THREADS_COUNT))
    if threads_count > 0:
        THREADS_COUNT = threads_count

    # Planet and costs:
    global PLANET_COASTS_GEOM_URL
    global PLANET_COASTS_RAWGEOM_URL
    PLANET_COASTS_GEOM_URL = os.path.join(PLANET_COASTS_URL, "latest_coasts.geom")
    PLANET_COASTS_RAWGEOM_URL = os.path.join(PLANET_COASTS_URL, "latest_coasts.rawgeom")

    if DEBUG:
        PLANET_URL = "https://www.dropbox.com/s/m3ru5tnj8g9u4cz/planet-latest.o5m?raw=1"
        PLANET_MD5_URL = (
            "https://www.dropbox.com/s/8wdl2hy22jgisk5/planet-latest.o5m.md5?raw=1"
        )
        NODE_STORAGE = "map"
        NEED_PLANET_UPDATE = False
