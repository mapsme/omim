import csv
import logging
from typing import Dict
from typing import List
from typing import Optional

import zss

from mwm.decode_id import decode_id

logger = logging.getLogger("diff_complex")


class Node(zss.Node):
    def __init__(self, label, children=None):
        self.parent = None
        super().__init__(label, children)

    def addpar(self, node):
        self.parent = node

    def __str__(self):
        return self.to_string_with_operations(operations={})

    def to_string_with_operations(self, operations):
        lines = []
        _print_tree(self, lines, is_root=True, operations=operations)
        return "".join(lines)

    def __hash__(self):
        return id(self)


def _print_tree(node, lines, prefix="", is_tail=True, is_root=False, operations=None):
    lines.append(prefix)
    if not is_root:
        if is_tail:
            lines.append("└───")
            prefix += "    "
        else:
            lines.append("├───")
            prefix += "│   "

    label = str(node.label).replace("\n", "")
    if operations is not None and node in operations:
        t = operations[node].type
        if t == zss.Operation.remove:
            lines.append("(-)")
        elif t == zss.Operation.insert:
            lines.append("(+)")
        elif t == zss.Operation.update:
            lines.append("(-+)")
    lines.append(label)
    lines.append("\n")

    l = len(node.children) - 1
    for i, c in enumerate(node.children):
        _print_tree(c, lines, prefix, i == l, False, operations)


def link_nodes(node: Node, parent: Node):
    node.addpar(parent)
    parent.addkid(node)


class HierarchyEntry:
    __slots__ = ("id", "parent_id", "depth", "lat", "lon", "type", "name", "country")

    @staticmethod
    def make_from_csv_row(csv_row: List[str]) -> Optional["HierarchyEntry"]:
        e = HierarchyEntry()
        if len(csv_row) == 8:
            e.id = csv_row[0]
            e.parent_id = csv_row[1]
            e.depth = int(csv_row[2])
            e.lat = float(csv_row[3])
            e.lon = float(csv_row[4])
            e.type = csv_row[5]
            e.name = csv_row[6]
            e.country = csv_row[7]
            return e
        # For old format:
        elif len(csv_row) == 6:
            e.id = csv_row[0]
            e.parent_id = csv_row[1]
            e.lat = float(csv_row[2])
            e.lon = float(csv_row[3])
            e.type = csv_row[4]
            e.name = csv_row[5]
            return e
        logger.error(f"Row [{csv_row}] - {len(csv_row)}  cannot be parsed.")
        return None

    def __eq__(self, other):
        if isinstance(other, (HierarchyEntry, str)):
            self_id = self.id
            other_id = other if isinstance(other, str) else other.id
            return self_id == other_id

        raise TypeError(f"{other}:{type(other)} is not supported.")

    def __str__(self):
        return (
            f"{self.id}[{self.type}]:{self.name} "
            f"({decode_id(self.id.split()[0] if ' ' in self.id else self.id)})"
        )


def read_complexes_from_csv(path: str) -> Dict[str, Node]:
    m = {}
    with open(path) as csvfile:
        rows = csv.reader(csvfile, delimiter=";")

        for row in rows:
            e = HierarchyEntry.make_from_csv_row(row)
            m[e.id] = Node(e)

    for id_, node in m.items():
        if node.label.parent_id:
            try:
                link_nodes(node, m[node.label.parent_id])
            except KeyError:
                logger.error(f"Id {node.label.parent_id} was not found in dict.")
                pass

    return m
