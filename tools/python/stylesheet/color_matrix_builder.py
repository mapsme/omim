#!/usr/bin/env python2.7

from __future__ import print_function
from style_comparator.matrix_builder import MatrixBuilder
from style_comparator.style_comparator import StyleComparator

import sys


def main():
    if len(sys.argv) < 2:
        print('Usage: {} drules_proto.bin [another_proto.bin [...]]'.format(sys.argv[0]))
        sys.exit(1)

    mb = MatrixBuilder()
    mb.add_files(sys.argv[1:])

    mb.parse_all_bins()

    mb.calculate_color_matrix()
    mb.write_all_matrices()
    mb.update_all_bins()
    mb.write_all_bins()

    sc = StyleComparator(matrix_builder=mb)
    sc.compare_styles()
    sc.write_changes_file()
    print("\nDone\n")

if __name__ == "__main__":
    main()
