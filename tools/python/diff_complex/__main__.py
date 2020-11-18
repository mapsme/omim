import argparse
import csv
import logging
import sys
from itertools import islice
from typing import Dict

import zss

from diff_complex.trees_builder import Node
from diff_complex.trees_builder import read_complexes_from_csv

logger = logging.getLogger("diff_complex")

csv.field_size_limit(sys.maxsize)


def parse_args():
    parser = argparse.ArgumentParser(description="Compare comples files.")
    parser.add_argument(
        "--old", metavar="PATH", type=str, help="Path to old file", required=True
    )
    parser.add_argument(
        "--new", metavar="PATH", type=str, help="Path to new file", required=True
    )

    parser.add_argument(
        "--popularity", metavar="PATH", type=str, help="Path to popularity file"
    )
    parser.add_argument(
        "--num",
        type=int,
        help="Number of objects from popularity file that to be compared",
        default=50,
    )

    parser.add_argument(
        "--threshold", type=int, help="Threshold of tree distance", default=1,
    )

    parser.add_argument(
        "--from_root",
        default=False,
        action="store_true",
        help="Compare trees from roots.",
    )

    return parser.parse_args()


def label_dist(a, b):
    if a == b:
        return 0
    else:
        return 1


def diff(
    old_complexes_map: Dict[str, Node],
    new_complexes_map: Dict[str, Node],
    id_: str,
    threshold: int = 1,
    from_root: bool = False,
):
    old_tree = old_complexes_map.get(id_)
    if old_tree is None:
        logger.warning(f"{id_} is not found in old complexes.")
        return

    new_tree = new_complexes_map.get(id_)
    if new_tree is None:
        logger.warning(f"{id_} is not found in new complexes.")
        return

    if from_root:
        p = old_tree.parent
        while p is not None:
            old_tree = p.parent

        p = new_tree.parent
        while p is not None:
            new_tree = p.parent

    operations = zss.simple_distance(
        old_tree, new_tree, label_dist=label_dist, return_operations=True
    )

    if operations[0] >= threshold:
        op_o = {
            o.arg1: o
            for o in operations[1]
            if o.type == zss.Operation.remove or o.type == zss.Operation.update
        }
        op_n = {
            o.arg2: o
            for o in operations[1]
            if o.type == zss.Operation.insert or o.type == zss.Operation.update
        }

        logger.warning(
            f"Differences found for id[{id_}]: distance is {operations[0]}\n"
            f"Old:\n"
            f"{old_tree.to_string_with_operations(op_o)}\n"
            f"New:\n"
            f"{new_tree.to_string_with_operations(op_n)}"
        )


def main():
    logging.basicConfig(
        level=logging.INFO, format="[%(asctime)s] %(levelname)s %(module)s %(message)s"
    )

    args = parse_args()
    old_complexes_map = read_complexes_from_csv(args.old)
    logger.info(f"{len(old_complexes_map)} old complexes was read from {args.old}")

    new_complexes_map = read_complexes_from_csv(args.new)
    logger.info(f"{len(new_complexes_map)} new complexes was read from {args.new}")

    if args.popularity:
        with open(args.popularity) as csvfile:
            rows = csv.reader(csvfile, delimiter=",")
            ids = [row[0] for row in islice(rows, args.num)]
    else:
        old_complexes_map = {
            k: v for k, v in old_complexes_map.items() if v.parent is None
        }
        new_complexes_map = {
            k: v for k, v in new_complexes_map.items() if v.parent is None
        }
        ids = list(old_complexes_map.keys())

    for id_ in ids:
        diff(old_complexes_map, new_complexes_map, id_, args.threshold, args.from_root)


main()
