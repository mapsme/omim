# Geomery library.

TARGET = geometry
TEMPLATE = lib
CONFIG += staticlib warn_on

ROOT_DIR = ..

include($$ROOT_DIR/common.pri)

SOURCES += \
  angles.cpp \
  distance_on_sphere.cpp \
  latlon.cpp \
  mercator.cpp \
  packer.cpp \
  region2d/binary_operators.cpp \
  robust_orientation.cpp \
  screenbase.cpp \
  spline.cpp \

HEADERS += \
  angles.hpp \
  any_rect2d.hpp \
  avg_vector.hpp \
  cellid.hpp \
  covering.hpp \
  covering_utils.hpp \
  distance.hpp \
  distance_on_sphere.hpp \
  latlon.hpp \
  mercator.hpp \
  packer.hpp \
  point2d.hpp \
  pointu_to_uint64.hpp \
  polygon.hpp \
  polyline2d.hpp \
  rect2d.hpp \
  rect_intersect.hpp \
  region2d.hpp \
  region2d/binary_operators.hpp \
  region2d/boost_concept.hpp \
  robust_orientation.hpp \
  screenbase.hpp \
  simplification.hpp \
  spline.hpp \
  transformations.hpp \
  tree4d.hpp \
  triangle2d.hpp \
