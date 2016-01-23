# Editor specific things.

TARGET = editor
TEMPLATE = lib
CONFIG += staticlib warn_on

ROOT_DIR = ..

include($$ROOT_DIR/common.pri)

SOURCES += \
  changeset_wrapper.cpp \
  opening_hours_ui.cpp \
  osm_auth.cpp \
  osm_utils.cpp \
  server_api.cpp \
  ui2oh.cpp \
  xml_feature.cpp \

HEADERS += \
  changeset_wrapper.hpp \
  opening_hours_ui.hpp \
  osm_auth.hpp \
  osm_utils.hpp \
  server_api.hpp \
  ui2oh.hpp \
  xml_feature.hpp \
