from collections import defaultdict
from itertools import chain
from protobuf_util import *
import re

PRIORITY_RE = re.compile("(.*)__(-?\d*)/(.*)")


class AbstractMatrixBuilder(object):
    def __init__(self):
        self.paths = []
        self.drules = []
        self.all_elements = defaultdict(lambda: defaultdict(lambda: -1))  # {path_index: {[(el.name, el.priority), (...)] : colour}}
        self.key_correspondences = defaultdict(dict)  # {path_index: {old_element_path: new_element_path, ...}, ...}


    # ADDING THE FILE:
    def add_file(self, filepath):
        print("Adding file: {}".format(filepath))
        self.paths.append(filepath)
        self.drules.append(read_bin(filepath))


    def add_files(self, files):
        for filepath in files:
            self.add_file(filepath)


    # CALCULATING
    def calculate_items_table(self):
        all_keys = chain.from_iterable(self.all_elements.itervalues())
        all_keys = frozenset(all_keys)

        colors_table = {}  # {key: (color_0, color_1, -1 if element absent in style 2, ...)}

        for key in all_keys:
            colors_table[key] = tuple(
                self.all_elements[path_index][key]
                for path_index in xrange(len(self.paths))
            )
        return colors_table


    # CORE
    def foreach_path(self, fn):
        map(fn, xrange(len(self.paths)))


    def recursive_iterator(self, sub_drules, index_list, index, worker_fn, filter_fn=has_color_fields):
        if filter_fn(sub_drules):
            worker_fn(sub_drules, index_list)

        for key, element in list_message_fields(sub_drules):
            self.recursive_iterator(element, index_list + key, index, worker_fn)


    def normalize_all_keys(self):
        self.foreach_path(self.normalize_keys_at_index)


    def normalize_keys_at_index(self, path_index):
        priority_key_transformation_dict = {}  # new_key: old_key
        new_priorities_dict = defaultdict(dict)  # {rest_of_key: {priority: color}}

        for key, val in self.all_elements[path_index].iteritems():

            priority_parts = PRIORITY_RE.findall(key)
            if priority_parts:
                rest_of_key, int_priority = self.make_priority_key_parts(priority_parts)
                new_priorities_dict[rest_of_key][int_priority] = val
                priority_key_transformation_dict[self.priority_key(rest_of_key, int_priority)] = key
            else:
                self.key_correspondences[path_index][key] = key

        for rest_of_key, priorities in new_priorities_dict.iteritems():
            for i, (priority, color) in enumerate(sorted(priorities.iteritems())):
                final_key = self.priority_key(rest_of_key, i)
                old_key = priority_key_transformation_dict[self.priority_key(rest_of_key, priority)]
                self.key_correspondences[path_index][old_key] = final_key
                del self.all_elements[path_index][old_key]
                self.all_elements[path_index][final_key] = color


    def make_priority_key_parts(self, priority_parts):
        priority_parts = priority_parts[0]
        rest_of_key = "{}/{}".format(priority_parts[0], priority_parts[2])
        int_priority = int(priority_parts[1])
        return rest_of_key, int_priority


    def priority_key(self, rest_of_key, i):
        return "{}~{}".format(rest_of_key, i)


    def item_key_generator(self, sub_drules, index_list, fn):
        return [
            self.path_key_and_value(sub_drules, index_list, color_key)
            for color_key in fn(sub_drules)
        ]


    def path_key_and_value(self, sub_drules, index_list, color_key):
        return (
            index_list + "/" + color_key,
            color_key,
            getattr(sub_drules, color_key)
        )
