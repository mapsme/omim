TARGET = partners_api_tests
CONFIG += console warn_on
CONFIG -= app_bundle
TEMPLATE = app

ROOT_DIR = ../..

INCLUDEPATH *= $$ROOT_DIR/3party/jansson/src

DEPENDENCIES = partners_api indexer platform coding geometry base jansson stats_client protobuf icu

include($$ROOT_DIR/common.pri)

DEFINES *= OMIM_UNIT_TEST_WITH_QT_EVENT_LOOP

QT *= core

macx-* {
  QT *= widgets # needed for QApplication with event loop, to test async events
  LIBS *= "-framework IOKit" "-framework SystemConfiguration"
}

win*|linux* {
  QT *= network
}

SOURCES += \
    $$ROOT_DIR/testing/testingmain.cpp \
    ads_engine_tests.cpp \
    booking_tests.cpp \
    cian_tests.cpp \
    facebook_tests.cpp \
    mopub_tests.cpp \
    rb_tests.cpp \
    taxi_engine_tests.cpp \
    uber_tests.cpp \
    viator_tests.cpp \
    yandex_tests.cpp \
