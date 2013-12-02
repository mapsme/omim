# Generates settings.ini for each bookmark in KMZ file

TARGET = kmz2ini
CONFIG += console warn_on
CONFIG -= app_bundle
TEMPLATE = app

ROOT_DIR = ../..
DEPENDENCIES = map gui search storage graphics indexer platform anim geometry coding base \
               freetype fribidi expat protobuf tomcrypt jansson zlib gflags

include($$ROOT_DIR/common.pri)

INCLUDEPATH *= $$ROOT_DIR/3party/gflags/src


QT *= core opengl

win32 {
  LIBS *= -lShell32
  win32-g++: LIBS *= -lpthread
}
macx-* {
  LIBS *= "-framework Foundation" "-framework IOKit"
}

SOURCES += \
    main.cpp \

#HEADERS += \
