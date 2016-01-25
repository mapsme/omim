TARGET = editor_tests
CONFIG += console warn_on
CONFIG -= app_bundle
TEMPLATE = app

ROOT_DIR = ../..
DEPENDENCIES = editor platform_tests_support platform geometry coding base stats_client opening_hours pugixml oauthcpp

include($$ROOT_DIR/common.pri)

QT *= core

HEADERS += \

SOURCES += \
    $$ROOT_DIR/testing/testingmain.cpp \
    opening_hours_ui_test.cpp \
    server_api_test.cpp \
    xml_feature_test.cpp \
    ui2oh_test.cpp \
    osm_auth_test.cpp \
