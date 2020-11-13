import logging
import os
from datetime import timedelta

import filelock
from airflow import DAG
from airflow.operators.python_operator import PythonOperator
from airflow.utils.dates import days_ago

from airmaps.instruments import settings
from airmaps.instruments import storage
from airmaps.instruments.utils import rm_build
from airmaps.instruments.utils import run_generation_from_first_stage
from maps_generator.generator import stages_declaration as sd
from maps_generator.generator.env import Env
from maps_generator.generator.env import PathProvider
from maps_generator.generator.env import get_all_countries_list
from maps_generator.generator.status import Status
from maps_generator.maps_generator import run_generation

logger = logging.getLogger("airmaps")


MAPS_STORAGE_PATH = os.path.join(settings.STORAGE_PREFIX, "maps")


class MapsGenerationDAG(DAG):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        build_prolog_task = PythonOperator(
            task_id="Build_prolog_task",
            provide_context=True,
            python_callable=MapsGenerationDAG.build_prolog,
            dag=self,
        )

        build_epilog_task = PythonOperator(
            task_id="Build_epilog_task",
            provide_context=True,
            python_callable=MapsGenerationDAG.build_epilog,
            dag=self,
        )

        for country in get_all_countries_list(PathProvider.borders_path()):
            build_prolog_task >> self.make_mwm_operator(country) >> build_epilog_task

    @staticmethod
    def get_params(namespace="env", **kwargs):
        return kwargs.get("params", {}).get(namespace, {})

    @staticmethod
    def publish_maps_build(env, subdir="temp_dir", **kwargs):
        subdir = MapsGenerationDAG.get_params(namespace="storage", **kwargs)[subdir]
        storage_path = os.path.join(MAPS_STORAGE_PATH, subdir)
        storage.wd_publish(
            env.paths.build_path, os.path.join(storage_path, env.build_name)
        )

    @staticmethod
    def fetch_maps_build(env, subdir="temp_dir", **kwargs):
        subdir = MapsGenerationDAG.get_params(namespace="storage", **kwargs)[subdir]
        storage_path = os.path.join(MAPS_STORAGE_PATH, subdir)
        storage.wd_fetch(
            os.path.join(storage_path, env.build_name), env.paths.build_path
        )

    @staticmethod
    def publish_map(env, country, subdir="temp_dir", **kwargs):
        subdir = MapsGenerationDAG.get_params(namespace="storage", **kwargs)[subdir]
        storage_path = os.path.join(MAPS_STORAGE_PATH, subdir)

        ignore_paths = {
            os.path.normpath(env.paths.draft_path),
            os.path.normpath(env.paths.generation_borders_path),
        }

        def publish(path):
            rel_path = path.replace(env.paths.build_path, "")[1:]
            dest = os.path.join(storage_path, env.build_name, rel_path)
            storage.wd_publish(path, dest)

        def find_and_publish_files_for_country(path):
            for root, dirs, files in os.walk(path):
                if os.path.normpath(root) in ignore_paths:
                    continue

                for dir in dirs:
                    if dir.startswith(country):
                        publish(os.path.join(root, dir))
                    else:
                        find_and_publish_files_for_country(os.path.join(root, dir))

                for file in files:
                    if file.startswith(country):
                        publish(os.path.join(root, file))

        find_and_publish_files_for_country(env.paths.build_path)

    @staticmethod
    def build_prolog(**kwargs):
        params = MapsGenerationDAG.get_params(**kwargs)
        env = Env(**params)
        kwargs["ti"].xcom_push(key="build_name", value=env.build_name)
        run_generation(
            env,
            (
                sd.StageDownloadAndConvertPlanet(),
                sd.StageCoastline(),
                sd.StagePreprocess(),
                sd.StageFeatures(),
                sd.StageDownloadDescriptions(),
            ),
        )
        env.clean()
        MapsGenerationDAG.publish_maps_build(env, **kwargs)
        rm_build(env)

    @staticmethod
    def make_build_mwm_func(country):
        def build_mwm(**kwargs):
            build_name = kwargs["ti"].xcom_pull(key="build_name")
            params = MapsGenerationDAG.get_params(**kwargs)
            params.update({"build_name": build_name, "countries": [country,]})

            lock_file = os.path.join(
                PathProvider.tmp_dir(), f"{build_name}-{__name__}-download.lock"
            )
            status_name = os.path.join(
                PathProvider.tmp_dir(), f"{build_name}-{__name__}-download.status"
            )
            with filelock.FileLock(lock_file):
                env = Env(**params)
                s = Status(status_name)
                if not s.is_finished():
                    MapsGenerationDAG.fetch_maps_build(env, **kwargs)
                    s.finish()

            # We need to check existing of mwm.tmp. It is needed if we want to
            # build mwms from part of planet.
            tmp_mwm_name = env.get_tmp_mwm_names()
            assert len(tmp_mwm_name) <= 1
            if not tmp_mwm_name:
                logger.warning(f"mwm.tmp does not exist for {country}.")
                return

            run_generation_from_first_stage(env, (sd.StageMwm(),), build_lock=False)
            MapsGenerationDAG.publish_map(env, country, **kwargs)

        return build_mwm

    @staticmethod
    def build_epilog(**kwargs):
        build_name = kwargs["ti"].xcom_pull(key="build_name")
        params = MapsGenerationDAG.get_params(**kwargs)
        params.update({"build_name": build_name})
        env = Env(**params)
        MapsGenerationDAG.fetch_maps_build(env, **kwargs)
        run_generation_from_first_stage(
            env,
            (
                sd.StageCountriesTxt(),
                sd.StageExternalResources(),
                sd.StageLocalAds(),
                sd.StageStatistics(),
                sd.StageCleanup(),
            ),
        )
        env.finish()
        MapsGenerationDAG.publish_maps_build(env, subdir="mwm_dir", **kwargs)
        rm_build(env)

    def make_mwm_operator(self, country):
        normalized_name = "__".join(country.lower().split())
        return PythonOperator(
            task_id=f"Build_country_{normalized_name}_task",
            provide_context=True,
            python_callable=MapsGenerationDAG.make_build_mwm_func(country),
            dag=self,
        )


PARAMS = {"storage": {"mwm_dir": "open_source", "temp_dir": "temp"}}
if settings.DEBUG:
    PARAMS["env"] = {
        # The planet file in debug mode does not contain Russia_Moscow territory.
        # It is needed for testing.
        "countries": ["Cuba", "Haiti", "Jamaica", "Cayman Islands", "Russia_Moscow"]
    }

OPEN_SOURCE_MAPS_GENERATION_DAG = MapsGenerationDAG(
    "Generate_open_source_maps",
    schedule_interval=timedelta(days=7),
    default_args={
        "owner": "MAPS.ME",
        "depends_on_past": True,
        "start_date": days_ago(0),
        "email": settings.EMAILS,
        "email_on_failure": True,
        "email_on_retry": False,
        "retries": 0,
        "retry_delay": timedelta(minutes=5),
        "priority_weight": 1,
        "params": PARAMS,
    },
)
