# Storage library tests.

TARGET = storage_integration_tests
CONFIG += console warn_on
CONFIG -= app_bundle
TEMPLATE = app

ROOT_DIR = ../..
DEPENDENCIES = storage indexer platform_tests_support platform geometry coding base jansson tomcrypt stats_client

include($$ROOT_DIR/common.pri)

DEFINES *= OMIM_UNIT_TEST_WITH_QT_EVENT_LOOP

QT *= core

macx-* {
  QT *= gui widgets # needed for QApplication with event loop, to test async events (downloader, etc.)
  LIBS *= "-framework IOKit" "-framework QuartzCore"
}
win32*|linux* {
  QT *= network
}

HEADERS += \

SOURCES += \
  ../../testing/testingmain.cpp \
  storage_http_tests.cpp \
