# OSM Editor tests.
TARGET = editor_tests
CONFIG += console warn_on
CONFIG -= app_bundle
TEMPLATE = app

ROOT_DIR = ../..

DEPENDENCIES = editor indexer base

include($$ROOT_DIR/common.pri)

SOURCES += ../../testing/testingmain.cpp \
    osm_merge_tests.cpp \
