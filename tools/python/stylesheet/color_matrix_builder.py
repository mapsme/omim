#!/usr/bin/env python2.7

from __future__ import print_function

import drules_struct_pb2 as proto
from image_maker import color_to_tuple, ImageMaker

import re
import sys
from collections import defaultdict
from os import path


ALLOWED_FIELDS_RE = re.compile("^[a-z]")
TYPE_RE = re.compile(".*Proto$")
DROP_NUMBER = re.compile("_\d*$")


def is_iterable(obj):
    if isinstance(obj, unicode):
        return False
    try:
        some_object_iterator = iter(obj)
        return True
    except TypeError:
        return False


class MatrixBuilder:
    def __init__(self):
        self.transformation_table = None  # {(color1, color2): [Element]}
        self.inverted_transformation_table = {}  # {element_key: Number_in_transformation_table}
        self.paths = []
        self.drules = []
        self.imgs = []
        self.all_elements = defaultdict(dict)  # {path_index: {[(el.name, el.priority), (...)] : colour}}
        self.possible_color_elements = ["color", "stroke_color", "text_color", "text_stroke_color"]


    def invert_transformation_table(self):
        for element, i in zip(self.transformation_table.iteritems(), range(0, len(self.transformation_table))):
            for key in element[1]:
                self.inverted_transformation_table[key] = i


    def worker(self, index_list, path_index, fn):
        keys = []  # [(el.name or type, priority)]
        element = self.drules[path_index]
        zoom = -1

        for index in index_list:
            priority = 0
            if is_iterable(element):
                element = element[index]
            else:
                if hasattr(element, "scale"):
                    zoom = element.scale
                key = dir(element)[index]
                key2 = ""
                if hasattr(element, "priority"):
                    priority = element.priority

                if hasattr(element, "name"):
                    key2 = "_" + element.name

                keys.append(("{}{}".format(key, key2), priority))
                element = getattr(element, key)

        fn(element, keys, zoom)


    def recursive_iterator(self, sub_drules, index_list, index, fn):
        """
        The function that iterates over the drules tree and applies the
        function to the elements that return true, when the self.filter_fn
         is applied to them.

         We assume that the tree will not be more than 10 levels deep, so we
         check whether we are too deep in recursion, which signals that we are
         in trouble (too much recursion).

        Args:
            sub_drules: The sub-tree of the drules. When you call this function
             from outside of this function, you most probably will pass
             the whole drules tree to it.
            index_list: A list of ints indicating the numeric indexes of found
             elements. When you call this function from outside of itself,
             pass and empty list.
            index: The index of the drules in the self.drules array
            fn: The worker function to be applied to the found elements.

        Returns:

        """
        if len(index_list) > 10:
            raise RuntimeError("Too much recursion")

        if is_iterable(sub_drules):
            for i in range(0, len(sub_drules)):
                index_list.append(i)
                if self.filter_fn(sub_drules[i]):
                    self.worker(index_list, index, fn)
                self.recursive_iterator(sub_drules[i], index_list, index, fn)
                del index_list[-1]
        else:
            if not TYPE_RE.match(type(sub_drules).__name__):
                return
            fields = dir(sub_drules)
            if fields:
                for i in range(0, len(fields)):
                    if callable(getattr(sub_drules, fields[i])) or (not ALLOWED_FIELDS_RE.match(fields[i])):
                        continue
                    index_list.append(i)
                    if self.filter_fn(getattr(sub_drules, fields[i])):
                        self.worker(index_list, index, fn)
                    self.recursive_iterator(getattr(sub_drules, fields[i]), index_list, index, fn)
                    del index_list[-1]


    def update_bins(self):
        print("Updating bin files and writing them")
        self.invert_transformation_table()

        def fn(index):
            self.update_bin(index)
            self.write_bin(index)

        self.foreach_path(fn)


    def write_bin(self, path_index):
        new_bin_name = self.paths[path_index]
        with open(new_bin_name, "wb") as outfile:
            outfile.write(self.drules[path_index].SerializeToString())


    def update_bin(self, index):
        self.recursive_iterator(self.drules[index], [], index, self.updating_worker_factory(index))


    def parse_bin(self, index):
        self.recursive_iterator(self.drules[index], [], index, self.reading_worker_factory(index))


    def add_file(self, filepath):
        print("Adding file: {}".format(filepath))
        self.paths.append(filepath)
        self.drules.append(self.read_bin(filepath))


    def foreach_path(self, fn):
        for i in range(0, len(self.paths)):
            fn(i)


    def parse_bins(self):
        print("Parsing the bin files")
        self.foreach_path(lambda index: self.parse_bin(index))


    def read_bin(self, filepath):
        drules = proto.ContainerProto()
        with open(filepath) as infile:
            drules.ParseFromString(infile.read())
        return drules


    def has_color_with_coords(self, element, color_el_name):
        if hasattr(element, color_el_name):
            if (hasattr(element, "{}_x".format(color_el_name)) and hasattr(element, "{}_y".format(color_el_name))):
                return True
            raise RuntimeError("Illformed style bin file. Element has color element {0}, but not color coordinate elements ({0}_x, {0}_y)".format(color_el_name))
        return False


    def filter_fn(self, element):
        if len(str(element)) == 0:
            return False
        for key in self.possible_color_elements:
            if self.has_color_with_coords(element, key):
                return True
        return False


    def key_maker(self, element, keys, zoom):
        new_key = []
        if hasattr(element, "priority"):
            new_key.append(("__", element.priority))

        key = str(zoom) + str(keys + new_key)

        for color_key in self.possible_color_elements:
            if not self.has_color_with_coords(element, color_key):
                continue

            postfix = color_key[:-(len("color"))]
            key = key + postfix
            yield(key, color_key)


    def reading_worker_factory(self, path_index):
        def reading_worker(element, keys, zoom):
            for key, color_key in self.key_maker(element, keys, zoom):
                self.all_elements[path_index][key] = getattr(element, color_key)

        return reading_worker


    def updating_worker_factory(self, path_index):
        def updating_worker(element, keys, zoom):
            for key, color_key in self.key_maker(element, keys, zoom):
                x, y = self.get_color_coords(path_index, key)

                setattr(element, "{}_x".format(color_key), x)
                setattr(element, "{}_y".format(color_key), y)

        return updating_worker


    def get_color_coords(self, path_index, key):
        color = self.all_elements[path_index][key]
        tup_color = color_to_tuple(color)  # todo use it to check that the colour is the same as in the bin file
        x, y = self.imgs[path_index].linear_index_to_coords(self.inverted_transformation_table[key])
        tup_color_from_img = self.imgs[path_index].img.getpixel((x, y))
        if (tup_color != tup_color_from_img):
            raise RuntimeError("Colors in the image and the bin file are diffrent")

        return x, y


    def calculate_color_matrix(self):
        print("Computing the color matrix")
        keys = []  # list of sets [{}]
        noncommon_keys = []  # list of sets [{}]
        first_iteration = True
        common_keys = set()
        for _, elements in self.all_elements.iteritems():
            keys.append(elements.keys())
            if first_iteration:
                common_keys = set(elements.keys())
                first_iteration = False
            else:
                common_keys &= set(elements.keys())

        for _, elements in self.all_elements.iteritems():
            noncommon_keys.append(set(elements.keys()) - common_keys)

        self.transformation_table = defaultdict(list)
        for key in common_keys:
            colors = []
            for path_index in range(0, len(self.all_elements)):
                colors.append(self.all_elements[path_index][key])
            colors = tuple(colors)
            self.transformation_table[colors].append(key)


    def write_matrices(self):
        print("Writing matrices")
        self.foreach_path(lambda index: self.write_matrix(index))


    def new_filename(self, bin_path, postfix):
        folder = path.dirname(bin_path)
        filename = path.basename(bin_path)[len("drules_proto"):-4]
        return path.join(folder, "static_colors{}.png".format(filename))


    def write_matrix(self, index):
        colors = map(lambda x: x[index], self.transformation_table)

        imgmk = ImageMaker(colors)
        imgmk.make()
        imgmk.save(self.new_filename(self.paths[index], ".png"))
        self.imgs.append(imgmk)


def main():
    if len(sys.argv) < 2:
        print('Usage: {} drules_proto.bin <another_proto.bin <...>>'.format(sys.argv[0]))
        sys.exit(1)

    mb = MatrixBuilder()
    for filepath in sys.argv[1:]:
        mb.add_file(filepath)
    mb.parse_bins()

    mb.calculate_color_matrix()
    mb.write_matrices()
    mb.update_bins()

    print("\nDone\n")

if __name__ == "__main__":
    main()
