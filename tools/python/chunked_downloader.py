# coding="utf-8"

import subprocess
import logging
import datetime
import sys
from os import path

CHUNK = 1000000


def setup_jenkins_console_logger(level=logging.INFO):
    stream_handler = logging.StreamHandler()
    stream_handler.setFormatter(logging.Formatter('%(asctime)s: %(msg)s'))
    logging.getLogger().setLevel(level)
    logging.getLogger().addHandler(stream_handler)

def shell(command, flags):
    spell = ["{0} {1}".format(command, flags)]
    process = subprocess.Popen(spell,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        shell=True
     )
    logging.getLogger().info(spell[0])
    out, err = process.communicate()
    if process.returncode != 0:
        logging.debug("Command ({}) return code: {}".format(command, process.returncode))
        raise IOError("Failed to download or save chunk")
    return filter(None, out.splitlines())


def curl_get_size(url):
    return int(
        shell(
        "curl",
        "-sI {url} | grep Content-Length | awk '{{print $2}}'".format(url=url))[0]
    )


def curl_range(url, start, end, target):
    shell(
        "curl",
        "--range {start}-{end} -o {target} {url}".format(
            start=start, end=end, target=target, url=url
        )
    )


def percent_done(size, done):
    return done / (size / 100.0)


class ChunkDownloader:
    def __init__(self, url, target):
        self.start = 0
        self.url = url
        self.size = curl_get_size(self.url)
        self.end = CHUNK - 1
        logging.info("The size is : {}".format(self.size))
        self.expected_chunks = self.size / CHUNK + 1
        self.start_time = None
        self.target = target

    def download(self):
        self.start_time = datetime.datetime.now()
        elapsed = 0
        go_on = True
        i = 1
        while go_on:
            if self.size <= CHUNK:
                go_on = False
                self.end = self.size
            if self.end >= self.size:
                go_on = False
                self.end = self.size

            logging.info("Curling from {} to {}".format(self.start, self.end))
            curl_range(
                self.url, self.start, self.end,
                path.join(self.target, "planet.part{num}".format(num=i))
            )
            elapsed = datetime.datetime.now() - self.start_time
            eta = elapsed / i * (self.expected_chunks - i)
            logging.info("Done {} of {} chunks; ETA: {}".format(i, self.expected_chunks, eta))
            self.start += CHUNK
            self.end = self.start + CHUNK - 1
            i += 1


    def glue(self):
        shell(
            "cat", "{} > {}".format(
                path.join(self.target, "planet.part?"),
                path.join(self.target, "planet.osm.pbf")
            )
        )


    def clean(self):
        shell(
            "rm",
            "-f {}".format(
                path.join(self.target, "planet.part*")
            )
        )


if __name__ == "__main__":
    setup_jenkins_console_logger()
    if len(sys.argv) < 3:
        logging.info("Usage: {} _url_ _target_".format(__file__))
        exit(1)

    url, target = sys.argv[1:3]

    ch = ChunkDownloader(url, target)
    ch.download()
    ch.glue()
    ch.clean()
