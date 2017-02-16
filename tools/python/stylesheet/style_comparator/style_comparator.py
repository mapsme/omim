from itertools import combinations

from operator import itemgetter
from os import path
import re
from protobuf_util import *
from abstract_matrix_builder import AbstractMatrixBuilder

FIND_TYPE_RE = re.compile("\{(.*)\}")

CLASS_BIT_MASKS = {
    "LineRuleProto": 1 << 2,
    "AreaRuleProto": 1 << 3,
    "SymbolRuleProto": 1 << 4,
    "CaptionRuleProto": 1 << 5,
    "PathTextRuleProto": 1 << 7,
    "ShieldRuleProto": 1 << 8
}


class StyleComparator(AbstractMatrixBuilder):
    def __init__(self, matrix_builder=None):
        AbstractMatrixBuilder.__init__(self)
        # This is to avoid reading bin files unnecessarily
        if matrix_builder:
            self.paths = matrix_builder.paths
            self.drules = matrix_builder.drules
        self.items_table = {}
        self.changes = []


    def comparator_reader_factory(self, index):
        def reader(sub_drules, index_list):
            for key, _, color in self.item_key_generator(sub_drules, index_list, non_color_fields):
                self.all_elements[index][key] = color
        return reader


    def compare_styles(self):
        print("Comparing styles")
        self.parse_bins()
        self.normalize_all_keys()
        self.items_table = self.calculate_items_table()
        index_combinations = combinations(range(len(self.paths)), 2)

        for comb in index_combinations:
            diff = self.compare(comb)
            self.changes.append(
                "{} {} {}".format(
                    path.basename(self.paths[comb[0]]),
                    path.basename(self.paths[comb[1]]),
                    diff
                )
            )


    def write_changes_file(self):
        print("Writing the changes file")
        filename = path.join(path.dirname(self.paths[0]), "changes.txt")
        with open(filename, "wb") as outfile:
            for ch in self.changes:
                outfile.write("{}\n".format(ch))


    def compare(self, indices):
        ind_1, ind_2 = indices

        diff_items = map(
            itemgetter(0),
            filter(
                lambda (key, val): val[ind_1] != val[ind_2],
                self.items_table.iteritems()
            )
        )

        all_protos = [
            x[0] for x in
            map(lambda s: FIND_TYPE_RE.findall(s), diff_items)
            if x
        ]

        return sum(map(lambda x: CLASS_BIT_MASKS[x], frozenset(all_protos)))


    def parse_one_bin(self, path_index):
        def filter_fun(sub_drules):
            return bool(non_color_fields(sub_drules))

        self.recursive_iterator(
            self.drules[path_index], "", path_index, self.comparator_reader_factory(path_index), filter_fn=filter_fun
        )


    def parse_bins(self):
        self.foreach_path(self.parse_one_bin)
