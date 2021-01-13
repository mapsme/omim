import logging
import os

from webdav3.client import Client
from webdav3.urn import Urn

from airmaps.instruments import settings

logger = logging.getLogger("airmaps")

WD_OPTIONS = {
    "webdav_hostname": settings.WD_HOST,
    "webdav_login": settings.WD_LOGIN,
    "webdav_password": settings.WD_PASSWORD,
}


def wd_fetch(src, dst):
    logger.info(f"Fetch form {src} to {dst} with options {WD_OPTIONS}.")
    client = Client(WD_OPTIONS)
    client.download_sync(src, dst)


def wd_publish(src, dst):
    logger.info(f"Publish form {src} to {dst} with options {WD_OPTIONS}.")
    if os.path.isdir(src):
        dst += Urn.separate

    dst = Urn(dst)
    tmp = f"{dst.path()}__"
    if dst.is_dir():
        tmp = f"{dst.path()[:-1]}__{Urn.separate}"

    parent = dst.parent()
    path = Urn.separate
    client = Client(WD_OPTIONS)
    for dir in str(parent).split(Urn.separate):
        if not client.check(path):
            client.mkdir(path)
        path += f"{dir}{Urn.separate}"

    client.upload_sync(local_path=src, remote_path=tmp)
    client.move(remote_path_from=tmp, remote_path_to=dst.path(), overwrite=True)
