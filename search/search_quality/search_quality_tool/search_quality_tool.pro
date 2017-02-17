# Search quality tool.

TARGET = search_quality_tool
CONFIG += console warn_on
CONFIG -= app_bundle
TEMPLATE = app

ROOT_DIR = ../../..
# todo(@m) revise
DEPENDENCIES = map drape_frontend traffic routing search_tests_support search search_quality storage indexer drape \
               platform editor geometry coding base freetype expat fribidi gflags \
               jansson protobuf osrm stats_client minizip succinct \
               opening_hours pugixml stb_image sdf_image

include($$ROOT_DIR/common.pri)

INCLUDEPATH *= $$ROOT_DIR/3party/gflags/src

# needed for Platform::WorkingDir() and unicode combining
QT *= core network opengl

macx-* {
  LIBS *= "-framework IOKit" "-framework SystemConfiguration"
}

HEADERS += \

SOURCES += \
    search_quality_tool.cpp \
