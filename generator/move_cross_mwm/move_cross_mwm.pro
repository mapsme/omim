# Moves cross_mwm section from older mwm to new one, cross-referencing features with osm2ft.

TARGET = move_cross_mwm
CONFIG += console warn_on
CONFIG -= app_bundle
TEMPLATE = app

ROOT_DIR = ../..
DEPENDENCIES = generator map routing traffic routing_common storage indexer \
               coding geometry base minizip succinct protobuf gflags stats_client icu

include($$ROOT_DIR/common.pri)

INCLUDEPATH *= $$ROOT_DIR/3party/gflags/src

QT *= core

macx-* {
    LIBS *= "-framework IOKit" "-framework SystemConfiguration"
}

SOURCES += \
    move_cross_mwm.cpp \
