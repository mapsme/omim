# Storage library tests.

TARGET = storage_integration_tests
CONFIG += console warn_on
CONFIG -= app_bundle
TEMPLATE = app

ROOT_DIR = ../..
DEPENDENCIES = map drape_frontend routing search storage tracking indexer drape partners_api platform_tests_support platform editor opening_hours geometry \
               coding base freetype expat fribidi tomcrypt jansson protobuf osrm stats_client \
               minizip succinct pugixml oauthcpp

include($$ROOT_DIR/common.pri)

DEFINES *= OMIM_UNIT_TEST_WITH_QT_EVENT_LOOP

QT *= core

macx-* {
  QT *= gui widgets network # needed for QApplication with event loop, to test async events (downloader, etc.)
  LIBS *= "-framework IOKit" "-framework QuartzCore" "-framework SystemConfiguration"
}
win32*|linux* {
  QT *= network
}

HEADERS += \
  test_defines.hpp

SOURCES += \
  ../../testing/testingmain.cpp \
  migrate_tests.cpp \
  storage_3levels_tests.cpp \
  storage_downloading_tests.cpp \
  storage_group_download_tests.cpp \
  storage_http_tests.cpp \
  storage_update_tests.cpp \
  test_defines.cpp \
