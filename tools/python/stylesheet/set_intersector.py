from __future__ import print_function

from collections import defaultdict


class Intersector:
    def __init__(self, *args, **kwargs):
        self.sets = list(args)

        self.all_keys = self.calculate_all_keys()
        self.table = self.build_table() # {key: [0, 1, 0, 1, ...]}, 1 if element present in ith set


    def build_table(self):
        if len(self.sets) == 0:
            return None

        table = defaultdict(list)
        for a_set in self.sets:
            for key in self.all_keys:
                table[key].append(1 if key in a_set else 0)

        return table


    def build_frequency_table(self):
        freq_table = [(x, sum(self.table[x])) for x in self.all_keys ]
        return freq_table


    def calculate_all_keys(self):
        if len(self.sets) == 0:
            return None

        all_keys = set()

        for a_set in enumerate(self.sets):
            all_keys |= a_set

        return all_keys


    def a(self):
        pass


    def find_common_elements(self):
        if len(self.sets) == 0:
            return None

        common = self.sets[0]
        if len(self.sets) == 1:
            return common

        for i in range(1, len(self.sets)):
            common &= self.sets[i]

        return common


if __name__ == "__main__":
    a = {"a", "b"}
    b = {"a", "b"}
    c = {"a", "d"}
    intersector = Intersector(a, b, c)
    intersector.build_frequency_table()
    a = intersector.find_common_elements()
    print("Hello")
