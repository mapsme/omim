# This subproject implements integration tests.
# This tests are launched on the whole world dataset.

# It is recommended to place tests here in the following cases:
# - tests are written to be launch on the whole world dataset;
# - tests covers significant number of subsystems;

TARGET = routing_integration_tests
CONFIG += console warn_on
CONFIG -= app_bundle
TEMPLATE = app

ROOT_DIR = ../..
DEPENDENCIES = generator_tests_support map routing search storage indexer platform \
               editor geometry coding base osrm jansson protobuf tomcrypt succinct \
               stats_client pugixml

DEPENDENCIES += opening_hours

macx-*: LIBS *= "-framework IOKit" "-framework SystemConfiguration"

include($$ROOT_DIR/common.pri)

QT *= core

SOURCES += \
  ../../testing/testingmain.cpp \
  bicycle_route_test.cpp \
  bicycle_turn_test.cpp \
  cross_section_tests.cpp \
  edge_index_consistency_tests.cpp \
  get_altitude_test.cpp \
  online_cross_tests.cpp \
  osrm_route_test.cpp \
  osrm_street_names_test.cpp \
  osrm_turn_test.cpp \
  pedestrian_route_test.cpp \
  routing_test_tools.cpp \

HEADERS += \
  routing_test_tools.hpp \
