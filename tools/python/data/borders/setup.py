#!/usr/bin/env python
import os
import sys
import tarfile

try:
    import lzma
except ImportError:
    from backports import lzma


module_dir = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(0, os.path.join(module_dir, "..", ".."))

from data.base import setup
from data.base import DATA_PATH


tar_path = os.path.join(DATA_PATH, "borders.tar")
tar_lzma_path = tar_path + ".xz"
try:
    with tarfile.open(os.path.join(DATA_PATH, "borders.tar"), "w") as tar:
        borders_path = os.path.join(DATA_PATH, "borders")
        for f in os.listdir(borders_path):
            tar.add(os.path.join(borders_path, f))

    with lzma.open(tar_lzma_path, "w") as f:
        with open(tar_path, "rb") as tar:
            f.write(tar.read())

    setup(
        __file__,
        "borders",
        ["borders.tar.xz", "packed_polygons.bin"],
        package_dir={"borders": ""},
        packages=["borders"],
    )
finally:
    for f in (tar_path, tar_lzma_path):
        if os.path.exists(f):
            os.remove(f)
