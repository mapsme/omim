#!/usr/bin/env python

import argparse
import logging

from utils import decode_routing_error, get_router, route_code_is_valid, ROUTE_TYPES


def build_route():
    router = get_router()
    code, distance = router.calculate_route(ROUTE_TYPES[args.type], *args.points)
    if route_code_is_valid(code):
        logging.debug(
            'Route calculated! Route length {distance}.'.format(distance=distance))
    else:
        error_name = decode_routing_error(code)
        logging.error(
            'Route calculation failed! Exit code \'{code}\''.format(distance=distance, code=error_name))
    return int(code)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Build route for points')
    parser.add_argument('-t', '--type', default='car', nargs='?', choices=ROUTE_TYPES.viewkeys())
    parser.add_argument('-p', '--points', help='4 coordinates -> startX startY finishX finishY', type=float,
                        default=None, nargs=4)
    args = parser.parse_args()

    exit(build_route())
