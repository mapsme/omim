from abstract_matrix_builder import AbstractMatrixBuilder
from collections import defaultdict
from protobuf_util import *
from image_maker import ImageMaker, argb_to_tuple
from os import path

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


class MatrixBuilder(AbstractMatrixBuilder):
    def __init__(self):
        AbstractMatrixBuilder.__init__(self)
        self.transformation_table = defaultdict(list)  # {(color1, color2): [Element]}
        self.inverted_transformation_table = {}  # {element_key: Number_in_transformation_table}
        self.imgs = []


    # READING:
    def reader_factory(self, index):
        def reader(sub_drules, index_list):
            for key, _, color in self.item_key_generator(sub_drules, index_list, color_fields):
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

        colors_table = self.calculate_items_table()
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
            for key, color_key, color in self.item_key_generator(sub_drules, index_list, color_fields):
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
        assert tup_color == tup_color_from_img, "Colors in the image and the bin file are different"
