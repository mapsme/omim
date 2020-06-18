""""
This file contains possible stages that maps_generator can run.
Some algorithms suppose a maps genration processes looks like:
    stage1, ..., stage_mwm[country_stage_1, ..., country_stage_M], ..., stageN
Only stage_mwm can contain country_
"""
import datetime
import json
import logging
import multiprocessing
import os
import shutil
import tarfile
from collections import defaultdict
from multiprocessing.pool import ThreadPool
from typing import AnyStr
from typing import Type

import maps_generator.generator.stages_tests as st
from descriptions.descriptions_downloader import check_and_get_checker
from descriptions.descriptions_downloader import download_from_wikidata_tags
from descriptions.descriptions_downloader import download_from_wikipedia_tags
from maps_generator.generator import coastline
from maps_generator.generator import settings
from maps_generator.generator import steps
from maps_generator.generator.env import Env
from maps_generator.generator.env import PathProvider
from maps_generator.generator.env import WORLD_COASTS_NAME
from maps_generator.generator.env import WORLD_NAME
from maps_generator.generator.exceptions import BadExitStatusError
from maps_generator.generator.gen_tool import run_gen_tool
from maps_generator.generator.stages import InternalDependency as D
from maps_generator.generator.stages import Stage
from maps_generator.generator.stages import Test
from maps_generator.generator.stages import country_stage
from maps_generator.generator.stages import depends_from_internal
from maps_generator.generator.stages import helper_stage_for
from maps_generator.generator.stages import mwm_stage
from maps_generator.generator.stages import outer_stage
from maps_generator.generator.stages import production_only
from maps_generator.generator.stages import stages
from maps_generator.generator.stages import test_stage
from maps_generator.generator.statistics import get_stages_info
from maps_generator.utils.file import download_files
from maps_generator.utils.file import is_verified
from post_generation.hierarchy_to_countries import hierarchy_to_countries
from post_generation.inject_promo_ids import inject_promo_ids
from post_generation.localads_mwm_to_csv import create_csv

logger = logging.getLogger("maps_generator")


def is_accepted(env: Env, stage: Type[Stage]) -> bool:
    return env.is_accepted_stage(stage)


@outer_stage
class StageDownloadAndConvertPlanet(Stage):
    def apply(self, env: Env, force_download: bool = True, **kwargs):
        if force_download or not is_verified(env.paths.planet_o5m):
            steps.step_download_and_convert_planet(
                env, force_download=force_download, **kwargs
            )


@outer_stage
class StageUpdatePlanet(Stage):
    def apply(self, env: Env, **kwargs):
        steps.step_update_planet(env, **kwargs)


@outer_stage
class StageCoastline(Stage):
    def apply(self, env: Env, use_old_if_fail=True):
        coasts_geom = "WorldCoasts.geom"
        coasts_rawgeom = "WorldCoasts.rawgeom"
        try:
            coastline.make_coastline(env)
        except BadExitStatusError as e:
            if not use_old_if_fail:
                raise e

            logger.info("Build costs failed. Try to download the costs...")
            download_files(
                {
                    settings.PLANET_COASTS_GEOM_URL: os.path.join(
                        env.paths.coastline_path, coasts_geom
                    ),
                    settings.PLANET_COASTS_RAWGEOM_URL: os.path.join(
                        env.paths.coastline_path, coasts_rawgeom
                    ),
                }
            )

        for f in [coasts_geom, coasts_rawgeom]:
            path = os.path.join(env.paths.coastline_path, f)
            shutil.copy2(path, env.paths.intermediate_data_path)


@outer_stage
class StagePreprocess(Stage):
    def apply(self, env: Env, **kwargs):
        steps.step_preprocess(env, **kwargs)


@outer_stage
@depends_from_internal(
    D(settings.HOTELS_URL, PathProvider.hotels_path, "p"),
    D(settings.PROMO_CATALOG_CITIES_URL, PathProvider.promo_catalog_cities_path, "p"),
    D(settings.POPULARITY_URL, PathProvider.popularity_path, "p"),
    D(settings.FOOD_URL, PathProvider.food_paths, "p"),
    D(settings.FOOD_TRANSLATIONS_URL, PathProvider.food_translations_path, "p"),
)
@test_stage(Test(st.make_test_booking_data(max_days=7), lambda e, _: e.production, True))
class StageFeatures(Stage):
    def apply(self, env: Env):
        extra = {}
        if is_accepted(env, StageDescriptions):
            extra.update({"idToWikidata": env.paths.id_to_wikidata_path})
        if env.production:
            extra.update(
                {
                    "booking_data": env.paths.hotels_path,
                    "promo_catalog_cities": env.paths.promo_catalog_cities_path,
                    "popular_places_data": env.paths.popularity_path,
                    "brands_data": env.paths.food_paths,
                    "brands_translations_data": env.paths.food_translations_path,
                }
            )
        if is_accepted(env, StageCoastline):
            extra.update({"emit_coasts": True})
        if is_accepted(env, StageIsolinesInfo):
            extra.update({"isolines_path": PathProvider.isolines_path()})

        steps.step_features(env, **extra)
        if os.path.exists(env.paths.packed_polygons_path):
            shutil.copy2(env.paths.packed_polygons_path, env.paths.mwm_path)


@outer_stage
@production_only
@helper_stage_for("StageDescriptions")
class StageDownloadDescriptions(Stage):
    def apply(self, env: Env):
        run_gen_tool(
            env.gen_tool,
            out=env.get_subprocess_out(),
            err=env.get_subprocess_out(),
            intermediate_data_path=env.paths.intermediate_data_path,
            user_resource_path=env.paths.user_resource_path,
            dump_wikipedia_urls=env.paths.wiki_url_path,
            idToWikidata=env.paths.id_to_wikidata_path,
            threads_count=settings.THREADS_COUNT,
        )

        langs = ("en", "ru", "es", "fr", "de")
        checker = check_and_get_checker(env.paths.popularity_path)
        download_from_wikipedia_tags(
            env.paths.wiki_url_path, env.paths.descriptions_path, langs, checker
        )
        download_from_wikidata_tags(
            env.paths.id_to_wikidata_path, env.paths.descriptions_path, langs, checker
        )


@outer_stage
@mwm_stage
class StageMwm(Stage):
    def apply(self, env: Env):
        with ThreadPool(settings.THREADS_COUNT) as pool:
            pool.map(
                lambda c: StageMwm.make_mwm(c, env),
                env.get_tmp_mwm_names(),
                chunksize=1,
            )

    @staticmethod
    def make_mwm(country: AnyStr, env: Env):
        if country == WORLD_NAME:
            StageIndex(country=country)(env)
            StageCitiesIdsWorld(country=country)(env)
        elif country == WORLD_COASTS_NAME:
            StageIndex(country=country)(env)
        else:
            StageIndex(country=country)(env)
            StageUgc(country=country)(env)
            StagePopularity(country=country)(env)
            StageSrtm(country=country)(env)
            StageIsolinesInfo(country=country)(env)
            StageDescriptions(country=country)(env)
            StageRouting(country=country)(env)
            StageRoutingTransit(country=country)(env)

        StageMwmStatistics(country=country)(env)
        env.finish_mwm(country)


@country_stage
@depends_from_internal(
    D(settings.UK_POSTCODES_URL, PathProvider.uk_postcodes_path, "p"),
    D(settings.US_POSTCODES_URL, PathProvider.us_postcodes_path, "p"),
)
class StageIndex(Stage):
    def apply(self, env: Env, country, **kwargs):
        if country == WORLD_NAME:
            steps.step_index_world(env, country, **kwargs)
        elif country == WORLD_COASTS_NAME:
            steps.step_coastline_index(env, country, **kwargs)
        else:
            if env.production:
                kwargs.update(
                    {
                        "uk_postcodes_dataset": env.paths.uk_postcodes_path,
                        "us_postcodes_dataset": env.paths.us_postcodes_path,
                    }
                )
            steps.step_index(env, country, **kwargs)


@country_stage
@production_only
class StageCitiesIdsWorld(Stage):
    def apply(self, env: Env, country, **kwargs):
        steps.step_cities_ids_world(env, country, **kwargs)


@country_stage
@depends_from_internal(D(settings.UGC_URL, PathProvider.ugc_path),)
@production_only
class StageUgc(Stage):
    def apply(self, env: Env, country, **kwargs):
        steps.step_ugc(env, country, **kwargs)


@country_stage
@production_only
class StagePopularity(Stage):
    def apply(self, env: Env, country, **kwargs):
        steps.step_popularity(env, country, **kwargs)


@country_stage
@production_only
class StageSrtm(Stage):
    def apply(self, env: Env, country, **kwargs):
        steps.step_srtm(env, country, **kwargs)


@country_stage
@production_only
class StageIsolinesInfo(Stage):
    def apply(self, env: Env, country, **kwargs):
        steps.step_isolines_info(env, country, **kwargs)


@country_stage
@production_only
class StageDescriptions(Stage):
    def apply(self, env: Env, country, **kwargs):
        steps.step_description(env, country, **kwargs)


@country_stage
class StageRouting(Stage):
    def apply(self, env: Env, country, **kwargs):
        steps.step_routing(env, country, **kwargs)


@country_stage
@depends_from_internal(D(settings.SUBWAY_URL, PathProvider.subway_path),
                       D(settings.TRANSIT_URL, PathProvider.transit_path_experimental),)
class StageRoutingTransit(Stage):
    def apply(self, env: Env, country, **kwargs):
        steps.step_routing_transit(env, country, **kwargs)


@country_stage
@helper_stage_for("StageStatistics")
class StageMwmStatistics(Stage):
    def apply(self, env: Env, country, **kwargs):
        steps.step_statistics(env, country, **kwargs)


@outer_stage
@depends_from_internal(
    D(
        settings.PROMO_CATALOG_COUNTRIES_URL,
        PathProvider.promo_catalog_countries_path,
        "p",
    )
)
class StageCountriesTxt(Stage):
    def apply(self, env: Env):
        countries = hierarchy_to_countries(
            env.paths.old_to_new_path,
            env.paths.borders_to_osm_path,
            env.paths.countries_synonyms_path,
            env.paths.hierarchy_path,
            env.paths.mwm_path,
            env.paths.mwm_version,
        )
        if env.production:
            countries_json = json.loads(countries)
            inject_promo_ids(
                countries_json,
                env.paths.promo_catalog_cities_path,
                env.paths.promo_catalog_countries_path,
                env.paths.mwm_path,
                env.paths.types_path,
                env.paths.mwm_path,
            )

            with open(env.paths.counties_txt_path, "w") as f:
                json.dump(countries_json, f, ensure_ascii=True, indent=1)


@outer_stage
class StageExternalResources(Stage):
    def apply(self, env: Env):
        black_list = {"00_roboto_regular.ttf"}
        resources = [
            os.path.join(env.paths.user_resource_path, file)
            for file in os.listdir(env.paths.user_resource_path)
            if file.endswith(".ttf") and file not in black_list
        ]
        for ttf_file in resources:
            shutil.copy2(ttf_file, env.paths.mwm_path)

        for file in os.listdir(env.paths.mwm_path):
            if file.startswith(WORLD_NAME) and file.endswith(".mwm"):
                resources.append(os.path.join(env.paths.mwm_path, file))

        resources.sort()
        with open(env.paths.external_resources_path, "w") as f:
            for resource in resources:
                fd = os.open(resource, os.O_RDONLY)
                f.write(f"{os.path.basename(resource)} {os.fstat(fd).st_size}\n")


@outer_stage
@production_only
class StageLocalAds(Stage):
    def apply(self, env: Env):
        create_csv(
            env.paths.localads_path,
            env.paths.mwm_path,
            env.paths.mwm_path,
            env.mwm_version,
            multiprocessing.cpu_count(),
        )
        with tarfile.open(f"{env.paths.localads_path}.tar.gz", "w:gz") as tar:
            for filename in os.listdir(env.paths.localads_path):
                tar.add(
                    os.path.join(env.paths.localads_path, filename), arcname=filename
                )


@outer_stage
class StageStatistics(Stage):
    def apply(self, env: Env):
        steps_info = get_stages_info(env.paths.log_path, {"statistics"})
        stats = defaultdict(lambda: defaultdict(dict))
        stats["steps"] = steps_info["steps"]
        for country in env.get_tmp_mwm_names():
            with open(os.path.join(env.paths.stats_path, f"{country}.json")) as f:
                stats["countries"][country] = {
                    "types": json.load(f),
                    "steps": steps_info["countries"][country],
                }

        def default(o):
            if isinstance(o, datetime.timedelta):
                return str(o)

        with open(os.path.join(env.paths.stats_path, "stats.json"), "w") as f:
            json.dump(
                stats, f, ensure_ascii=False, sort_keys=True, indent=2, default=default
            )


@outer_stage
class StageCleanup(Stage):
    def apply(self, env: Env):
        logger.info(
            f"osm2ft files will be moved from {env.paths.mwm_path} "
            f"to {env.paths.osm2ft_path}."
        )
        for x in os.listdir(env.paths.mwm_path):
            p = os.path.join(env.paths.mwm_path, x)
            if os.path.isfile(p) and x.endswith(".mwm.osm2ft"):
                shutil.move(p, os.path.join(env.paths.osm2ft_path, x))

        logger.info(f"{env.paths.draft_path} will be removed.")
        shutil.rmtree(env.paths.draft_path)


stages.init()
