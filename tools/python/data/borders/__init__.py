import os
import sys
import site
import tarfile
import logging

try:
    import lzma
except ImportError:
    from backports import lzma

logger = logging.getLogger(__name__)

_omim_data_dir = "omim-data"
DATA_PATH = os.path.join(sys.prefix, _omim_data_dir)
if not os.path.exists(DATA_PATH):
    DATA_PATH = os.path.join(site.USER_BASE, _omim_data_dir)

if not os.path.exists(DATA_PATH):
    logger.error("{} was not found.".format(DATA_PATH))
    sys.exit(1)

borders_path = os.path.join(DATA_PATH, "borders")
if not os.path.exists(os.path.join(DATA_PATH, "borders")):
    tar_path = os.path.join(DATA_PATH, "borders.tar")
    tar_lzma_path = os.path.join(DATA_PATH, "borders.tar.xz")

    with open(tar_lzma_path, "rb") as f:
        with open(tar_path, "wb") as tar:
            decompressed = lzma.decompress(f.read())
            tar.write(decompressed)

    with tarfile.open(tar_path, "r") as tar:
        tar.extractall(borders_path)

    if os.path.exists(tar_path):
        os.remove(tar_path)

    logger.info("{} was created.".format(borders_path))
