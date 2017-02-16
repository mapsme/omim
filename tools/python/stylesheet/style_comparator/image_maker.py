from PIL import Image
from math import sqrt, ceil
from itertools import product


def div_with_remainder(x):
    return x / 256, x % 256


def argb_to_tuple(int_color):
    ret_list = [0, 0, 0, 0]

    if int_color < 0:
        return tuple(ret_list)

    for i in range(3, -1, -1):
        a, b = div_with_remainder(int_color)
        int_color = a
        ret_list[i] = b
        if int_color == 0:
            break
    assert int_color == 0, "The input number was too big"

    alpha = 255 - ret_list[0]
    return tuple(ret_list[1:] + [alpha])


class ImageMaker:
    def __init__(self, colors):
        self.colors = colors
        self.side_length = self.calculate_side_length(len(self.colors))
        self.img = None


    def calculate_min_length(self, num):
        min_len = int(ceil(sqrt(num)))

        if min_len > 4096:
            raise ValueError("Side for the matrix is too big")

        return min_len


    def calculate_side_length(self, num):
        """
        Calculate the side length for our colour matrix given the number of pixels.
        The matrix must be square and the side length must be a power of 2, so
        we need to find such number that is higher or equal to the minimum lengh,
        but is a power of 2. Also, for each colour we have squares of 2x2 pixels.
        Args:
            num: Number of pixels in the colour matrix

        Returns: side length that is a power of 2
        """

        min_len = self.calculate_min_length(num)
        i = 8
        while i < min_len:
            i *= 2

        return i * 2 # because we store squares 2x2 for each color


    def make(self):
        img = Image.new("RGBA", (self.side_length, self.side_length))
        limit = self.side_length / 2
        for x, y in product(range(limit), repeat=2):
            index = x * limit + y
            if index >= len(self.colors):
                self.img = img
                return

            color = argb_to_tuple(self.colors[index])

            img.putpixel((y * 2, x * 2), color)
            img.putpixel((y * 2+1, x * 2), color)
            img.putpixel((y * 2, x * 2+1), color)
            img.putpixel((y * 2+1, x * 2+1), color)
        self.img = img


    def save(self, fname):
        self.img.save(fname, "PNG")


    def linear_index_to_coords(self, linear_index):
        half_size = self.side_length / 2
        x = (linear_index % half_size) * 2 + 1
        y = (linear_index / half_size) * 2 + 1
        return x, y
