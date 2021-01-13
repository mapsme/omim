import logging
import os
import shutil
from datetime import datetime
from typing import Iterable

from maps_generator.generator.env import Env
from maps_generator.generator.stages import Stage
from maps_generator.generator.stages import get_stage_name
from maps_generator.maps_generator import run_generation

logger = logging.getLogger("airmaps")


def put_current_date_in_filename(filename):
    path, name = os.path.split(filename)
    parts = name.split(".", maxsplit=1)
    parts[0] += f"__{datetime.today().strftime('%Y_%m_%d')}"
    return os.path.join(path, ".".join(parts))


def get_latest_filename(filename, prefix=""):
    path, name = os.path.split(filename)
    parts = name.split(".", maxsplit=1)
    assert len(parts) != 0, parts
    parts[0] = f"{prefix}latest"
    return os.path.join(path, ".".join(parts))


def rm_build(env):
    logger.info(f"Build {env.build_path} will be removed.")
    shutil.rmtree(env.build_path)


def run_generation_from_first_stage(
    env: Env, stages: Iterable[Stage], build_lock: bool = True
):
    from_stage = get_stage_name(next(iter(stages)))
    run_generation(env, stages, from_stage, build_lock)
