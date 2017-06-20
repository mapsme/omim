# UGC library.

TARGET = ugc
TEMPLATE = lib
CONFIG += staticlib warn_on

ROOT_DIR = ..

include($$ROOT_DIR/common.pri)

HEADERS += \
    api.hpp \
    serdes.hpp \
    types.hpp \

SOURCES += \
    api.cpp \
