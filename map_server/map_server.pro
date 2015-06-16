TARGET = MapsWithMe-server
CONFIG   += console warn_on
CONFIG   -= app_bundle
TEMPLATE = app

DEFINES += QJSONRPC_BUILD

DEPENDENCIES = map drape_frontend routing search storage indexer drape platform geometry coding base \
               osrm bzip2 freetype expat fribidi tomcrypt jansson protobuf qjsonrpc gflags stats_client zlib succinct

ROOT_DIR = ..
include($$ROOT_DIR/common.pri)

target.path = /usr/bin
INSTALLS += target

QT       += core gui opengl network

INCLUDEPATH *= $$ROOT_DIR/3party/qjsonrpc/src
INCLUDEPATH *= $$ROOT_DIR/3party/gflags/src

macx* {
  LIBS *= "-framework Foundation" "-framework CoreWLAN" \
          "-framework QuartzCore" "-framework IOKit"
}
win32* : LIBS *= -lshell32

SOURCES += \
    main.cpp \

HEADERS += \
    main.hpp \
