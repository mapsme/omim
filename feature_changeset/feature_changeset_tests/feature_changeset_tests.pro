#-------------------------------------------------
#
# Project created by QtCreator 2015-04-07T17:26:27
#
#-------------------------------------------------

TARGET = feature_changeset_tests
CONFIG += console warn_on
CONFIG -= app_bundle
TEMPLATE = app

ROOT_DIR = ../..
DEPENDENCIES = stats_client feature_changeset platform coding base expat

include($$ROOT_DIR/common.pri)

QT *= core

macx-* {
  LIBS *= "-framework Foundation"
}

INCLUDEPATH *= $$ROOT_DIR/3party/expat/lib

HEADERS += \

SOURCES += \
    ../../testing/testingmain.cpp \
    feature_changeset_tests.cpp \
    osm_data_tests.cpp \
    osm_entity_tests.cpp \
