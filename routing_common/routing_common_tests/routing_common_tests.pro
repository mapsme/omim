# Routing common lib unit tests

TARGET = routing_common_tests
CONFIG += console warn_on
CONFIG -= app_bundle
TEMPLATE = app

ROOT_DIR = ../..
DEPENDENCIES = routing_common indexer platform editor geometry coding base \
               protobuf succinct jansson stats_client pugixml stats_client icu

macx-*: LIBS *= "-framework IOKit" "-framework SystemConfiguration"

include($$ROOT_DIR/common.pri)

INCLUDEPATH += $$ROOT_DIR/3party/jansson/src

QT *= core

HEADERS += \
  transit_tools.hpp \

SOURCES += \
  ../../testing/testingmain.cpp \
  vehicle_model_for_country_test.cpp \
  vehicle_model_test.cpp \
