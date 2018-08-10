#!/usr/bin/env python

import subprocess
import time
import argparse
import logging

try:
    import psutil, pygal
except ImportError:
    print('Please install dependencies \'pip install psutil pygal\' first')
    exit(1)

from utils import (parse_config, parse_section, decode_routing_error,
                   route_code_is_valid, SCRIPT_NAME, CONFIG_NAME)

logging.basicConfig(level=logging.DEBUG, format='%(asctime)s %(levelname)s %(message)s')


def plot_to_file(name, memory_points, timestamps):
    line_chart = pygal.Line(y_title='{name} - Memory usage in MB'.format(name=name), x_label_rotation=20)
    line_chart.x_labels = map(str, timestamps)
    line_chart.add('Memory', memory_points)
    line_chart.range = [min(memory_points), max(memory_points)]
    filename = '{}.svg'.format(name)
    line_chart.render_to_file(filename)
    return filename


def check_memory_usage(pid):
    mem = psutil.Process(pid).memory_info()[0] / float(2 ** 20)
    return mem, time.strftime('%H:%M:%S', time.gmtime())


def profile_memory_usage(route_name):
    _, _, route_type, point_args = parse_section(route_name, config)
    cmd = 'python {name} -p {coords} -t {route_type}'.format(name=SCRIPT_NAME, coords=point_args,
                                                             route_type=route_type)
    process = subprocess.Popen([cmd], shell=True, stderr=subprocess.PIPE)
    memory_measures = []
    time_measures = []

    # While process is running
    while process.poll() is None:
        memory_measure, timestamp = check_memory_usage(process.pid)
        memory_measures.append(memory_measure)
        time_measures.append(timestamp)
        logging.debug('Memory usage {memory_usage} MB'.format(memory_usage=memory_measure))
        time.sleep(args.timeout)

    code = process.poll()
    if not (route_code_is_valid(code)):
        error_code = decode_routing_error(code)
        logging.error('Error occurred while building route! Error code: {code}'.format(code=error_code))
        return

    plot_to_file(args.route, memory_measures, time_measures)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Build route for points')
    parser.add_argument('-r', '--route', type=str, help='Route name from routes.ini',
                        default='MoscowGreatLukiPedestrian')
    parser.add_argument('-t', '--timeout', type=float, help='Timeout between measures in seconds', default=1)
    args = parser.parse_args()

    config = parse_config()

    if args.route in config.sections():
        profile_memory_usage(args.route)
    else:
        logging.error('Route name \'{name}\' not found in \'{config}\''.format(name=args.route, config=CONFIG_NAME))
