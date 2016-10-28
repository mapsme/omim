# Map library tests.

TARGET = map_tests
CONFIG += console warn_on
CONFIG -= app_bundle
TEMPLATE = app

ROOT_DIR = ../..
DEPENDENCIES = map drape_frontend routing search storage tracking drape indexer partners_api platform editor geometry coding base \
               freetype fribidi expat protobuf tomcrypt jansson osrm stats_client minizip succinct pugixml stats_client

DEPENDENCIES *= opening_hours

drape {
  DEPENDENCIES *= drape_frontend drape
}

include($$ROOT_DIR/common.pri)

DEFINES *= OMIM_UNIT_TEST_WITH_QT_EVENT_LOOP

QT *= core opengl

macx-* {
  QT *= gui widgets network # needed for QApplication with event loop, to test async events (downloader, etc.)
  LIBS *= "-framework IOKit" "-framework QuartzCore" "-framework SystemConfiguration"
}

win*|linux* {
  QT *= network
}

win32*: LIBS *= -lOpengl32
macx-*: LIBS *= "-framework IOKit" "-framework SystemConfiguration"

SOURCES += \
  ../../testing/testingmain.cpp \
  address_tests.cpp \
  bookmarks_test.cpp \
  chart_generator_tests.cpp \
  feature_getters_tests.cpp \
  ge0_parser_tests.cpp \
  geourl_test.cpp \
  gps_track_collection_test.cpp \
  gps_track_storage_test.cpp \
  gps_track_test.cpp \
  kmz_unarchive_test.cpp \
  mwm_url_tests.cpp \

!linux* {
  SOURCES += working_time_tests.cpp \
}
