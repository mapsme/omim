# Map library.

TARGET = map
TEMPLATE = lib
CONFIG += staticlib warn_on

ROOT_DIR = ..

INCLUDEPATH *= $$ROOT_DIR/3party/protobuf/src $$ROOT_DIR/3party/expat/lib

include($$ROOT_DIR/common.pri)

HEADERS += \
    active_maps_layout.hpp \
    bookmark.hpp \
    bookmark_manager.hpp \
    country_tree.hpp \
    feature_vec_model.hpp \
    ge0_parser.hpp \
    geourl_process.hpp \
    mwm_url.hpp \
    route_track.hpp \
    routing_session.hpp \
    storage_bridge.hpp \
    track.hpp \
    user_mark.hpp \
    user_mark_container.hpp \
    framework.hpp \

SOURCES += \
    ../api/src/c/api-client.c \
    active_maps_layout.cpp \
    address_finder.cpp \
    bookmark.cpp \
    bookmark_manager.cpp \
    country_tree.cpp \
    framework.cpp \
    ge0_parser.cpp \
    geourl_process.cpp \
    mwm_url.cpp \
    route_track.cpp \
    routing_session.cpp \
    storage_bridge.cpp \
    track.cpp \
    user_mark.cpp \
    user_mark_container.cpp \
    feature_vec_model.cpp \

!iphone*:!tizen*:!android* {
  QT += opengl
}
