from __future__ import print_function

import json
import logging
from os import makedirs
from os.path import join, exists
from argparse import ArgumentParser
from sys import argv
from Util import find_omim

from shutil import rmtree
import subprocess

logging.basicConfig(format='%(asctime)s %(message)s', level=logging.DEBUG)

LAST_DOWNLOADED_URL_FILE = "last_downloaded.url"


class MwmDownloader:
    def __init__(self):
        self.omim_root = find_omim()
        self.datapath = join(self.omim_root, "..", "data")
        self.url_path = join(self.datapath, LAST_DOWNLOADED_URL_FILE)
        #"http://eu1.mapswithme.com/direct/160128/"
        self.link = None
        self.process_cli()
        if not self.link:
            self.link = self.link_from_countries_txt()

        self.var_should_download = self.should_download(self.link)
        logging.debug("should_download = {val}".format(val=self.var_should_download))


    def link_from_countries_txt(self):
        with open(join(self.omim_root, "data", "countries.txt")) as countries:
            data = json.load(countries)

        version = data["v"]
        return "http://direct.mapswithme.com/direct/{}/".format(version)


    def read_url_from_file(self, url_file):
        with open(url_file, "r") as the_file:
            return the_file.readline()


    def should_download(self, link):
        if not exists(self.datapath):
            return True

        if not exists(self.url_path):
            return True

        if self.read_url_from_file(self.url_path) != link:
            return True

        return False


    def process_cli(self):
        parser = ArgumentParser()
        parser.add_argument("-u", "--url", dest="link", help="The url from which the MWMs should be downloaded")

        args = parser.parse_args()

        self.link = args.link
        logging.info("The link is: {}".format(self.link))


    def _delete_all_data(self):
        if exists(self.datapath):
            rmtree(self.datapath)
        if not exists(self.datapath):
            makedirs(self.datapath)

    def _download_new_data(self):
        process = subprocess.Popen(
            "wget -nv --recursive --no-parent --no-directories --directory-prefix={prefix} {link}".format(
                prefix=self.datapath, link=self.link
            ),
            shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )

        downloaded = 0

        while True:
            line = process.stderr.readline()
            if not line:
                break
            downloaded += 1
            logging.debug("{downloaded} >> {line}".format(downloaded=downloaded, line=line))

        with open(self.url_path, "w") as url_file:
            url_file.write(self.link)


    def download(self):
        logging.info("Do we need to download? -- {}".format("Yes" if self.var_should_download else "No"))
        if self.var_should_download:
            self._delete_all_data()
            self._download_new_data()


if __name__ == "__main__":
    d = MwmDownloader()
    d.download()
