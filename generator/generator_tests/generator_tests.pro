TARGET = generator_tests
CONFIG += console warn_on
CONFIG -= app_bundle
TEMPLATE = app

ROOT_DIR = ../..
DEPENDENCIES = generator_tests_support platform_tests_support generator drape_frontend routing search storage \
               indexer drape map platform editor geometry \
               coding base freetype expat fribidi tomcrypt jansson protobuf osrm stats_client \
               minizip succinct pugixml tess2 gflags oauthcpp


include($$ROOT_DIR/common.pri)

QT *= core

HEADERS += \
    source_data.hpp \
    types_helper.hpp \

SOURCES += \
    ../../testing/testingmain.cpp \
    altitude_test.cpp \
    check_mwms.cpp \
    coasts_test.cpp \
    edge_index_test.cpp \
    feature_builder_test.cpp \
    feature_merger_test.cpp \
    metadata_parser_test.cpp \
    osm2meta_test.cpp \
    osm_id_test.cpp \
    osm_o5m_source_test.cpp \
    osm_type_test.cpp \
    source_data.cpp \
    source_to_element_test.cpp \
    srtm_parser_test.cpp \
    tag_admixer_test.cpp \
    tesselator_test.cpp \
    triangles_tree_coding_test.cpp \
