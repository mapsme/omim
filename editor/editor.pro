# OSM Editor core library.
TARGET = editor
TEMPLATE = lib
CONFIG += staticlib warn_on

ROOT_DIR = ..

include($$ROOT_DIR/common.pri)

#INCLUDEPATH *=

SOURCES += \
    $$ROOT_DIR/3party/pugixml/src/pugixml.cpp \

HEADERS += \
    $$ROOT_DIR/3party/pugixml/src/pugixml.hpp \
    $$ROOT_DIR/3party/pugixml/src/pugiconfig.hpp \
