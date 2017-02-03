#!/usr/bin/env python2.7

from __future__ import print_function

import drules_struct_pb2 as proto
from image_maker import argb_to_tuple, ImageMaker

import re
import sys
from collections import defaultdict
from os import path


ZOOM_RE = re.compile("/\d*?/<")
PRIORITY_RE = re.compile("(.*)/__(-?\d*)/(.*)")
STRIP_DIGITS_RE = re.compile("/\d*$")


def fields(sub_drules):
    if not is_protobuf(sub_drules):
        return []

    return sub_drules.DESCRIPTOR.fields


def color_fields(sub_drules):
    color_field_names = filter(lambda x: x.endswith("color"), map(lambda x: x.name, fields(sub_drules)))
    return filter(lambda  x: sub_drules.HasField(x), color_field_names)


def new_is_iterable(sub_drules):
    typ = type(sub_drules).__name__
    return typ.startswith("Repeated")


def is_protobuf(sub_drules):
    return hasattr(sub_drules, "DESCRIPTOR")


def proper_join(lst):
    return "/".join(map(lambda x: str(x), lst))


class MatrixBuilder:
    def __init__(self):
        self.transformation_table = None  # {(color1, color2): [Element]}
        self.inverted_transformation_table = {}  # {element_key: Number_in_transformation_table}
        self.key_corespondences = defaultdict(dict) # {path_index: {unnormalized_key: normalized_key}}
        self.paths = []
        self.drules = []
        self.imgs = []
        self.all_elements = defaultdict(dict)  # {path_index: {[(el.name, el.priority), (...)] : colour}}


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
            for key, color_key in self.color_key_generator(sub_drules, index_list):
                self.all_elements[index][key] = getattr(sub_drules, color_key)
        return reader


    def parse_bins(self):
        print("Parsing the bin files")
        self.foreach_path(lambda index: self.parse_bin(index))


    def parse_bin(self, index):
        self.recursive_iterator(self.drules[index], [], index, self.reader_factory(index))
        self.normalize_keys(index)


    def normalize_keys(self, path_index):
        priority_key_transformation_dict = {}  # new_key: old_key

        keys_to_delete = []

        new_keys_dict = {}  # str -> color
        new_priorities_dict = defaultdict(dict)  # {rest_of_key: {priority: color}}

        for old_key, val in self.all_elements[path_index].iteritems():
            key = old_key
            if ZOOM_RE.findall(key):
                new_key = ZOOM_RE.sub("/<", key)
                new_keys_dict[new_key] = val
                keys_to_delete.append(key)
                key = new_key
                self.key_corespondences[path_index][old_key] = key

            priority_parts = PRIORITY_RE.findall(key)
            if priority_parts:
                rest_of_key, int_priority = self.make_priority_key_parts(priority_parts)
                new_priorities_dict[rest_of_key][int_priority] = val
                keys_to_delete.append(key)
                del (new_keys_dict[key])
                del (self.key_corespondences[path_index][old_key])
                priority_key_transformation_dict[self.priority_key(rest_of_key, int_priority)] = old_key

        for rest_of_key, priorities in new_priorities_dict.iteritems():
            for i, (priority, color) in enumerate(sorted(priorities.iteritems())):
                final_key = self.priority_key(rest_of_key, i)
                new_keys_dict[final_key] = color
                self.key_corespondences[path_index][
                    priority_key_transformation_dict[self.priority_key(rest_of_key, priority)]] = final_key

        self.update_all_elements(keys_to_delete, new_keys_dict, path_index)


    def make_priority_key_parts(self, priority_parts):
        priority_parts = priority_parts[0]
        rest_of_key = "{}/{}".format(STRIP_DIGITS_RE.sub("", priority_parts[0]), priority_parts[2])
        int_priority = int(priority_parts[1])
        return rest_of_key, int_priority


    def update_all_elements(self, keys_to_delete, new_keys_dict, path_index):
        for key in keys_to_delete:
            self.all_elements.pop(key, None)
        self.all_elements[path_index].update(new_keys_dict)


    def priority_key(self, rest_of_key, i):
        return "{}~{}".format(rest_of_key, i)


    #CALCULATE COLOR MATRIX
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
            for path_index, _ in enumerate(self.all_elements):
                colors.append(self.all_elements[path_index][key])
            colors = tuple(colors)
            self.transformation_table[colors].append(key)


    #WRITE COLOR MATRICES
    def write_matrices(self):
        print("Writing matrices")
        self.foreach_path(lambda index: self.write_matrix(index))


    def write_matrix(self, index):
        colors = map(lambda x: x[index], self.transformation_table)

        imgmk = ImageMaker(colors)
        imgmk.make()
        imgmk.save(self.new_filename(self.paths[index], ".png"))
        self.imgs.append(imgmk)


    def new_filename(self, bin_path, postfix):
        folder = path.dirname(bin_path)
        filename = self.calculate_style_type_postfix(bin_path)
        return path.join(folder, "static_colors{}.png".format(filename))


    def calculate_style_type_postfix(self, bin_path):
        return path.basename(bin_path)[len("drules_proto"):-4]


    #UPDATE BINS
    def update_bins(self):
        print("Updating bin files and writing them")
        self.invert_transformation_table()

        def fn(index):
            self.update_bin(index)
            self.write_bin(index)

        self.foreach_path(fn)


    def invert_transformation_table(self):
        for element, (i, _) in zip(self.transformation_table.iteritems(), enumerate(self.transformation_table)):
            for key in element[1]:
                self.inverted_transformation_table[key] = i


    def update_bin(self, index):
        self.recursive_iterator(self.drules[index], [], index, self.updater_factory(index))


    def write_bin(self, path_index):
        new_bin_name = self.paths[path_index]
        with open(new_bin_name, "wb") as outfile:
            outfile.write(self.drules[path_index].SerializePartialToString())

        with open(new_bin_name[:-4] + ".txt", "wb") as outfile:
            outfile.write(unicode(self.drules[path_index]))


    def updater_factory(self, path_index):
        def updater(sub_drules, index_list):
            if not sub_drules.ByteSize():  # Protobuf returns uninitialized optional fields as well, we need to ignore them
                return

            for key, color_key in self.color_key_generator(sub_drules, index_list):
                if key not in self.key_corespondences[path_index]:
                    print("Possible duplicate entry in bin file. \n{}".format(key))
                    continue
                key = self.key_corespondences[path_index][key]
                x, y = self.get_color_coords(path_index, key)

                setattr(sub_drules, "{}_x".format(color_key), x)
                setattr(sub_drules, "{}_y".format(color_key), y)

                if not self.drules[path_index].IsInitialized():
                    error_fields = sub_drules.FindInitializationErrors()
                    for ef in error_fields:
                        setattr(sub_drules, ef, getattr(sub_drules, ef))

        return updater


    def get_color_coords(self, path_index, key):
        color = self.all_elements[path_index][key]
        tup_color = argb_to_tuple(color)  # todo use it to check that the colour is the same as in the bin file
        x, y = self.imgs[path_index].linear_index_to_coords(self.inverted_transformation_table[key])
        tup_color_from_img = self.imgs[path_index].img.getpixel((x, y))
        if (tup_color != tup_color_from_img):
            raise RuntimeError("Colors in the image and the bin file are diffrent")

        return x, y


    #CORE
    def foreach_path(self, fn):
        for i, _ in enumerate(self.paths):
            fn(i)

    def color_key_generator(self, sub_drules, index_list):
        for color_key in color_fields(sub_drules):
            yield proper_join(index_list[3:] + [color_key]), color_key


    def make_key(self, element): #todo find a better name for it
        key = self.make_key_part(element, "name", "{}")
        key += self.make_key_part(element, "scale", "<{}>")
        key += self.make_key_part(element, "priority", "__{}")
        key += self.make_key_part(element, "apply_if", "_?_{}")
        return key


    def make_key_part(self, element, field, formatr):
        try:
            strattr = getattr(element, field)
            return formatr.format(strattr) if strattr or strattr == 0 else ""
        except AttributeError:
            return ""
        except Exception as e:
            print("Caught unexpected error: {}".format(e.__class__.__name__))
            raise RuntimeError()


    def filter_fn(self, sub_drules):
        return is_protobuf(sub_drules) and len(color_fields(sub_drules)) > 0


    def recursive_iterator(self, sub_drules, index_list, index, fn):
        index_list.append(self.make_key(sub_drules))
        if self.filter_fn(sub_drules):
            fn(sub_drules, index_list)

        for field_name, element, field_descriptor in map(lambda x: (x.name, getattr(sub_drules, x.name), x),  fields(sub_drules)):
            index_list.append(field_name)

            if new_is_iterable(element):
                for j, sub_drule in enumerate(element):
                    index_list.append(j)
                    self.recursive_iterator(sub_drule, index_list, index, fn)
                    index_list.pop()
            else:
                if not is_protobuf(element):
                    index_list.pop()
                    continue
                self.recursive_iterator(element, index_list, index, fn)
            index_list.pop()
        index_list.pop()


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
