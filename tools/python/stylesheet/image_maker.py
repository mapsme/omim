from PIL import Image
from math import sqrt


def argb_to_tuple(int_color):
    str_color = "{0:0{1}x}".format(int_color, 8)
    alpha = str_color[:2]
    color = str_color[2:]

    ret = []
    for i in range(0, 6, 2):
        ret.append(int(color[i: i + 2], 16))
    ret.append(255 - int(alpha, 16))

    return tuple(ret)


class ImageMaker:
    def __init__(self, colors):
        self.colors = colors
        self.side_length = self.calculate_side_length(len(self.colors))
        self.img = None


    def calculate_min_length(self, num):
        min_len = sqrt(num)
        if min_len != int(min_len):
            min_len += 1
        min_len = int(min_len)

        if min_len > 4096:
            raise RuntimeError("Side for the matrix is too big")

        return min_len


    def calculate_side_length(self, num):
        min_len = self.calculate_min_length(num)
        i = 8
        while i < min_len:
            i *= 2

        return i * 2 # because we store squares 2x2 for each color


    def make(self):
        img = Image.new("RGBA", (self.side_length, self.side_length))
        limit = self.side_length / 2
        for x in range(0, limit):
            for y in range(0, limit):
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
