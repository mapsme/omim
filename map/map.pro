# Map library.

TARGET = map
TEMPLATE = lib
CONFIG += staticlib warn_on

ROOT_DIR = ..

INCLUDEPATH *= $$ROOT_DIR/3party/protobuf/src $$ROOT_DIR/3party/freetype/include

include($$ROOT_DIR/common.pri)

HEADERS += \
    active_maps_layout.hpp \
    alfa_animation_task.hpp \
    anim_phase_chain.hpp \
    animator.hpp \
    api_mark_container.hpp \
    api_mark_point.hpp \
    benchmark_engine.hpp \
    benchmark_provider.hpp \
    bookmark.hpp \
    bookmark_manager.hpp \
    change_viewport_task.hpp \
    compass_arrow.hpp \
    country_status_display.hpp \
    country_tree.hpp \
    feature_vec_model.hpp \
    framework.hpp \
    ge0_parser.hpp \
    geourl_process.hpp \
    information_display.hpp \
    location_state.hpp \
    move_screen_task.hpp \
    mwm_url.hpp \
    navigator.hpp \
    navigator_utils.hpp \
    pin_click_manager.hpp \
    rotate_screen_task.hpp \
    ruler.hpp \
    styled_point.hpp \
    track.hpp \
    user_mark.hpp \
    user_mark_container.hpp \
    user_mark_dl_cache.hpp \

SOURCES += \
    ../api/src/c/api-client.c \
    active_maps_layout.cpp \
    address_finder.cpp \
    alfa_animation_task.cpp \
    anim_phase_chain.cpp \
    animator.cpp \
    api_mark_container.cpp \
    benchmark_engine.cpp \
    benchmark_provider.cpp \
    bookmark.cpp \
    bookmark_manager.cpp \
    change_viewport_task.cpp \
    compass_arrow.cpp \
    country_status_display.cpp \
    country_tree.cpp \
    feature_vec_model.cpp \
    framework.cpp \
    framework_editor.cpp \
    ge0_parser.cpp \
    geourl_process.cpp \
    information_display.cpp \
    location_state.cpp \
    move_screen_task.cpp \
    mwm_url.cpp \
    navigator.cpp \
    navigator_utils.cpp \
    pin_click_manager.cpp \
    rotate_screen_task.cpp \
    ruler.cpp \
    styled_point.cpp \
    track.cpp \
    user_mark_container.cpp \
    user_mark_dl_cache.cpp \

!iphone*:!tizen*:!android* {
  HEADERS += qgl_render_context.hpp
  SOURCES += qgl_render_context.cpp
  QT += opengl
}
