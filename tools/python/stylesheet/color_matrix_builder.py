#!/usr/bin/env python2.7

from __future__ import print_function

import drules_struct_pb2 as proto

import re
import sys

from collections import defaultdict
from image_maker import argb_to_tuple, ImageMaker
from itertools import chain

from os import path
from protobuf_util import *

PRIORITY_RE = re.compile("(.*)__(-?\d*)/(.*)")


"""
In order to be able to quickly switch between different styles, we need to do as
little work for switching as possible. Thus, if the only difference between the
styles is the colour palette, we can just switch a colour matrix, and the map
colours will change.

So we need to create colour matrices in such a way that identical features in
different styles correspond to pixels at the same locations in their
corresponding colour matrices.

Here is how we do that:

 1) We read the binary style files

 2) We recursively find all the elements that have attributes, whose names end in
    "color", storing the path tho those elements

 3) We combine all the keys (paths) from all the styles in one set and build
    a table:

                         style_1 | style_2 | style_n
    cont/path/one        color_1 | -1      | color_2
    cont/path/two        color_2 | color_3 | color_2
    cont/path/three      color_2 | color_3 | color_2

    Where "color_x" stands for actual colour value, and -1 means that the
    corresponding key is not present in the corresponding style.

 4) We invert this table, making the colours the key of the new table, and
    we append paths to those colour transitions to the list of values:

    (color_1, -1, color_2): cont/path/one
    (color_2, color_3, color_2): cont/path/two, cont/path/three,

    From this table we infer, that the matrix for style_1 will have pixels with
    color_1 and color_2, for style_2: -1 and color_3, style_3: color_2 and color_2,

    so:

 5) We iterate over this table and write the n'th elements to the corresponding
    matrices.


 6) Now that we know which of the style elements (or features) got which pixel
    in the resulting colour matrices, we need to write the colour coordinates
    (the coordinates of those pixels) to the initial binary style file. So we
    iterate over the binary file again, we build the path keys again, and ask
    our table for the index of that key in the transformation table. Then we ask
    the image instance to convert that number into the actual coordinates.
"""

class MatrixBuilder:
    def __init__(self):
        self.transformation_table = defaultdict(list)  # {(color1, color2): [Element]}
        self.inverted_transformation_table = {}  # {element_key: Number_in_transformation_table}
        self.paths = []
        self.drules = []
        self.imgs = []
        self.all_elements = defaultdict(lambda: defaultdict(lambda: -1))  # {path_index: {[(el.name, el.priority), (...)] : colour}}
        self.key_correspondences = defaultdict(dict)  # {path_index: {old_element_path: new_element_path, ...}, ...}


    # ADDING THE FILE:
    def add_file(self, filepath):
        print("Adding file: {}".format(filepath))
        self.paths.append(filepath)
        self.drules.append(self.read_bin(filepath))


    def read_bin(self, filepath):
        drules = proto.ContainerProto()
        with open(filepath) as infile:
            drules.ParseFromString(infile.read())
        return drules


    # READING:
    def reader_factory(self, index):
        def reader(sub_drules, index_list):
            for key, _, color in self.color_key_generator(sub_drules, index_list):
                self.all_elements[index][key] = color
        return reader


    def parse_all_bins(self):
        print("Parsing the bin files")
        self.foreach_path(self.parse_bin)


    def parse_bin(self, index):
        self.recursive_iterator(self.drules[index], "", index, self.reader_factory(index))


    # CALCULATE COLOR MATRIX
    def calculate_color_matrix(self):
        self.normalize_all_keys()

        all_keys = chain.from_iterable(self.all_elements.itervalues())
        all_keys = frozenset(all_keys)
        colors_table = defaultdict(list)  # {key: [color_0, color_1, -1 if element absent in style 2, ...]}

        for key in all_keys:
            colors_table[key] = tuple(
                self.all_elements[path_index][key]
                for path_index in xrange(len(self.paths))
            )

        for key, colors in colors_table.iteritems():
            self.transformation_table[colors].append(key)


    # WRITE COLOR MATRICES
    def write_all_matrices(self):
        print("Writing matrices")
        self.foreach_path(self.write_matrix)


    def write_matrix(self, index):
        colors = map(lambda x: x[index], self.transformation_table)

        imgmk = ImageMaker(colors)
        imgmk.make()
        imgmk.save(self.new_filename(self.paths[index], "png"))
        self.imgs.append(imgmk)


    def new_filename(self, bin_path, postfix):
        folder = path.dirname(bin_path)
        style_name = self.calculate_style_type_postfix(bin_path)
        return path.join(folder, "static_colors{}.{}".format(style_name, postfix))


    def calculate_style_type_postfix(self, bin_path):
        return path.basename(bin_path)[len("drules_proto"):-4]


    # UPDATE BINS
    def invert_transformation_table(self):
        for element, i in zip(
                self.transformation_table.iteritems(),
                xrange(len(self.transformation_table))
        ):
            for key in element[1]:
                self.inverted_transformation_table[key] = i


    def update_all_bins(self):
        print("Updating bin files")
        self.invert_transformation_table()
        self.foreach_path(self.update_one_bin)


    def update_one_bin(self, index):
        self.recursive_iterator(
            self.drules[index], "", index, self.updater_factory(index)
        )


    def write_all_bins(self):
        print("Writing bin files")
        self.foreach_path(self.write_one_bin)


    def write_one_bin(self, path_index):
        new_bin_name = self.paths[path_index]
        with open(new_bin_name, "wb") as outfile:
            outfile.write(self.drules[path_index].SerializePartialToString())

        txt_filename = path.splitext(new_bin_name)[0] + ".txt"
        with open(txt_filename, "wb") as outfile:
            outfile.write(unicode(self.drules[path_index]))


    def updater_factory(self, path_index):
        def updater(sub_drules, index_list):
            for key, color_key, color in self.color_key_generator(sub_drules, index_list):
                key_2 = self.key_correspondences[path_index][key]
                x, y = self.get_color_coords(path_index, key_2)
                self.verify_color_at_coords(path_index, (x, y), color)

                setattr(sub_drules, "{}_x".format(color_key), x)
                setattr(sub_drules, "{}_y".format(color_key), y)

        return updater


    def get_color_coords(self, path_index, key):
        return self.imgs[path_index].linear_index_to_coords(
            self.inverted_transformation_table[key]
        )


    def verify_color_at_coords(self, path_index, xy, orig_color):
        tup_color = argb_to_tuple(orig_color)
        tup_color_from_img = self.imgs[path_index].img.getpixel(xy)
        assert tup_color == tup_color_from_img, "Colors in the image and the bin file are diffrent"


    # CORE
    def foreach_path(self, fn):
        map(fn, xrange(len(self.paths)))


    def color_key_generator(self, sub_drules, index_list):
        return [
            self.path_key_and_color(sub_drules, index_list, color_key)
            for color_key in color_fields(sub_drules)
        ]


    def path_key_and_color(self, sub_drules, index_list, color_key):
        return (
            index_list + "/" + color_key,
            color_key,
            getattr(sub_drules, color_key)
        )


    def recursive_iterator(self, sub_drules, index_list, index, worker_fn):
        if has_color_fields(sub_drules):
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


def main():
    if len(sys.argv) < 2:
        print('Usage: {} drules_proto.bin [another_proto.bin [...]]'.format(sys.argv[0]))
        sys.exit(1)

    mb = MatrixBuilder()
    for filepath in sys.argv[1:]:
        mb.add_file(filepath)
    mb.parse_all_bins()

    mb.calculate_color_matrix()
    mb.write_all_matrices()
    mb.update_all_bins()
    mb.write_all_bins()

    print("\nDone\n")

if __name__ == "__main__":
    main()
