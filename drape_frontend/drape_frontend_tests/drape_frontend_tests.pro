
TARGET = drape_frontend_tests
CONFIG += console warn_on
CONFIG -= app_bundle
TEMPLATE = app

DEPENDENCIES = drape_frontend drape platform indexer geometry coding base expat stats_client stb_image sdf_image

ROOT_DIR = ../..
include($$ROOT_DIR/common.pri)

QT *= opengl

macx-* {
  LIBS *= "-framework CoreLocation" "-framework Foundation" "-framework CoreWLAN" \
          "-framework QuartzCore" "-framework IOKit" "-framework Cocoa" "-framework SystemConfiguration"
}
win32*|linux* {
  QT *= network
}

SOURCES += \
  ../../testing/testingmain.cpp \
  navigator_test.cpp \
  object_pool_tests.cpp \
  user_event_stream_tests.cpp \

