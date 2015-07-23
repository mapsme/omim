TARGET = render
TEMPLATE = lib
CONFIG += staticlib warn_on

ROOT_DIR = ..

include($$ROOT_DIR/common.pri)

INCLUDEPATH *= $$ROOT_DIR/3party/protobuf/src $$ROOT_DIR/3party/expat/lib $$ROOT_DIR/3party/freetype/include

SOURCES += \
    agg_curves.cpp \
    anim/angle_interpolation.cpp \
    anim/anyrect_interpolation.cpp \
    anim/controller.cpp \
    anim/segment_interpolation.cpp \
    anim/task.cpp \
    anim/value_interpolation.cpp \
    basic_tiling_render_policy.cpp \
    country_status_display.cpp \
    coverage_generator.cpp \
    cpu_drawer.cpp \
    default_font.cpp \
    drawer.cpp \
    engine.cpp \
    events.cpp \
    feature_info.cpp \
    feature_processor.cpp \
    feature_styler.cpp \
    geometry_processors.cpp \
    gpu_drawer.cpp \
    information_display.cpp \
    location_state.cpp \
    navigator.cpp \
    navigator_utils.cpp \
    proto_to_styles.cpp \
    queued_renderer.cpp \
    render_policy.cpp \
    route_renderer.cpp \
    route_shape.cpp \
    scales_processor.cpp \
    simple_render_policy.cpp \
    software_renderer.cpp \
    text_engine.cpp \
    tile.cpp \
    tile_cache.cpp \
    tile_renderer.cpp \
    tile_set.cpp \
    tiler.cpp \
    tiling_render_policy_mt.cpp \
    tiling_render_policy_st.cpp \
    window_handle.cpp \
    yopme_render_policy.cpp \
    alfa_animation_task.cpp \
    compass_arrow.cpp \
    ruler.cpp \
    animator.cpp \
    move_screen_task.cpp \
    rotate_screen_task.cpp \
    active_maps_bridge.cpp

HEADERS += \
    anim/angle_interpolation.hpp \
    anim/anyrect_interpolation.hpp \
    anim/controller.hpp \
    anim/segment_interpolation.hpp \
    anim/task.hpp \
    anim/value_interpolation.hpp \
    area_info.hpp \
    basic_tiling_render_policy.hpp \
    coverage_generator.hpp \
    cpu_drawer.hpp \
    drawer.hpp \
    engine.hpp \
    events.hpp \
    feature_info.hpp \
    feature_processor.hpp \
    feature_styler.hpp \
    frame_image.hpp \
    geometry_processors.hpp \
    gpu_drawer.hpp \
    navigator.hpp \
    navigator_utils.hpp \
    path_info.hpp \
    point.h \
    proto_to_styles.hpp \
    queued_renderer.hpp \
    rect.h \
    render_policy.hpp \
    route_renderer.hpp \
    route_shape.hpp \
    scales_processor.hpp \
    simple_render_policy.hpp \
    software_renderer.hpp \
    text_engine.h \
    tile.hpp \
    tile_cache.hpp \
    tile_renderer.hpp \
    tile_set.hpp \
    tiler.hpp \
    tiling_render_policy_mt.hpp \
    tiling_render_policy_st.hpp \
    window_handle.hpp \
    yopme_render_policy.hpp \
