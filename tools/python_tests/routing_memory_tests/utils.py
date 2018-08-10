import subprocess
import ConfigParser
import os
import sys

try:
    import pyomimrouting
except ImportError:
    print('Please build pyomimrouting first:\n'
          'cmake {OMIM_ROOT} -DPYBINDINGS=ON\n'
          'make pyomimrouting')
    exit(1)


ROOT = os.path.dirname(os.path.abspath(__file__))
CONFIG_NAME = os.path.join(ROOT, 'routes.ini')
DATA_PATH = os.path.join(ROOT, '../../../data')
ROUTE_TYPES = pyomimrouting.route_types.names
SCRIPT_NAME = os.path.join(ROOT, 'build_route.py')
TIME_COMMAND = '/usr/bin/time'


def check_executable_exist(exec_name):
    return subprocess.call('type {name}'.format(name=exec_name), shell=True,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE) == 0


if sys.platform == 'darwin':
    # Note: install gtime with 'brew install gnu-time'
    TIME_COMMAND = 'gtime'
    if not check_executable_exist(TIME_COMMAND):
        print('Please, install gnu-time first \'brew install gnu-time\'')


def get_router():
    return pyomimrouting.PyRouter(DATA_PATH)


def route_code_is_valid(code):
    return int(code) == pyomimrouting.codes.NoError


def decode_routing_error(error_code):
    if error_code in pyomimrouting.codes.values:
        return pyomimrouting.codes.values[error_code]
    return error_code


def measure_memory_usage(args_tuple):
    route_name, expected_memory_peak, route_type, coords = args_tuple
    cmd = '{time_command} -f "%M" python {name} -p {coords} -t {route_type}'.format(
        time_command=TIME_COMMAND, name=SCRIPT_NAME, coords=coords, route_type=route_type)

    process = subprocess.Popen([cmd], shell=True, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    # Last line in stderr will contain output from '/usr/bin/time'
    memory_peak = int(stderr.splitlines()[-1]) / 1024
    expected_memory_peak = int(expected_memory_peak)
    code = process.poll()
    if route_code_is_valid(code) and memory_peak > expected_memory_peak:
        code = -1
        memory_peak -= expected_memory_peak
    return route_name, code, memory_peak


def parse_section(section_name, config):
    route_type = config.get(section_name, 'type')
    start_x, start_y = config.get(section_name, 'start').replace(' ', '').split(',')
    finish_x, finish_y = config.get(section_name, 'finish').replace(' ', '').split(',')
    expected_memory_peak = config.get(section_name, 'expected_memory_peak')
    point_args = ' '.join((start_x, start_y, finish_x, finish_y))
    return section_name, expected_memory_peak, route_type, point_args


def parse_config(config_name=CONFIG_NAME):
    config = ConfigParser.ConfigParser()
    config.read(config_name)
    return config

