TARGET = feature_changeset
TEMPLATE = lib
CONFIG += staticlib

ROOT_DIR = ..

INCLUDEPATH *= $$ROOT_DIR/3party/protobuf/src $$ROOT_DIR/3party/expat/lib

include($$ROOT_DIR/common.pri)

SOURCES += featurechangeset.cpp \
    featurechangeset_upload.cpp \
    osm_data.cpp \
    sqlite3.c \
    osm_entity.cpp \

HEADERS += featurechangeset.hpp \
    osm_data.hpp \
    sqlite3.h \
    sqlite3ext.h \
    osm_entity.hpp \
