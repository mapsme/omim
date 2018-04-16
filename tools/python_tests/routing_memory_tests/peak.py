#!/usr/bin/env python

import multiprocessing
import logging

from utils import (measure_memory_usage, parse_config, parse_section,
                   route_code_is_valid, decode_routing_error)

logging.basicConfig(level=logging.DEBUG, format='%(asctime)s %(levelname)s %(message)s')


def calculate_peak():
    config = parse_config()
    args_tuple = (parse_section(section, config) for section in config.sections())
    pool = multiprocessing.Pool()
    exit_code = 0
    for name, code, memory_usage_count in pool.imap_unordered(measure_memory_usage, args_tuple):
        if route_code_is_valid(code):
            logging.debug(
                'Route name: {name} built, memory usage peak {usage} MB'.format(name=name, usage=memory_usage_count))
        elif code == -1:
            logging.error(
                'Route name: {name} memory exceeded on {usage} MB!'.format(name=name, usage=memory_usage_count))
            exit_code = 1
        else:
            error_name = decode_routing_error(code)
            logging.error(
                'Error occur while building route {name}: {error_name}'.format(name=name, error_name=error_name))
            exit_code = 1
    return exit_code


if __name__ == '__main__':
    exit(calculate_peak())
