TARGET = drape_frontend
TEMPLATE = lib
CONFIG += staticlib

DEPENDENCIES = drape base
ROOT_DIR = ..
include($$ROOT_DIR/common.pri)

INCLUDEPATH *= $$ROOT_DIR/3party/protobuf/src
DEFINES += DRAW_INFO

SOURCES += \
    animation/base_interpolator.cpp \
    animation/interpolation_holder.cpp \
    animation/interpolations.cpp \
    animation/model_view_animation.cpp \
    apply_feature_functors.cpp \
    area_shape.cpp \
    backend_renderer.cpp \
    base_renderer.cpp \
    batchers_pool.cpp \
    circle_shape.cpp \
    drape_engine.cpp \
    engine_context.cpp \
    frontend_renderer.cpp \
    line_shape.cpp \
    map_data_provider.cpp \
    memory_feature_index.cpp \
    message_acceptor.cpp \
    message_queue.cpp \
    navigator.cpp \
    path_symbol_shape.cpp \
    path_text_shape.cpp \
    poi_symbol_shape.cpp \
    read_manager.cpp \
    read_mwm_task.cpp \
    render_group.cpp \
    rule_drawer.cpp \
    stylist.cpp \
    text_layout.cpp \
    text_shape.cpp \
    threads_commutator.cpp \
    tile_info.cpp \
    tile_key.cpp \
    tile_tree.cpp \
    tile_tree_builder.cpp \
    tile_utils.cpp \
    user_mark_shapes.cpp \
    user_marks_provider.cpp \
    viewport.cpp \
    visual_params.cpp \
    my_position.cpp \
    user_event_stream.cpp \

HEADERS += \
    animation/base_interpolator.hpp \
    animation/interpolation_holder.hpp \
    animation/interpolations.hpp \
    animation/model_view_animation.hpp \
    apply_feature_functors.hpp \
    area_shape.hpp \
    backend_renderer.hpp \
    base_renderer.hpp \
    batchers_pool.hpp \
    circle_shape.hpp \
    drape_engine.hpp \
    engine_context.hpp \
    frontend_renderer.hpp \
    intrusive_vector.hpp \
    line_shape.hpp \
    map_data_provider.hpp \
    map_shape.hpp \
    memory_feature_index.hpp \
    message.hpp \
    message_acceptor.hpp \
    message_queue.hpp \
    message_subclasses.hpp \
    navigator.hpp \
    path_symbol_shape.hpp \
    path_text_shape.hpp \
    poi_symbol_shape.hpp \
    read_manager.hpp \
    read_mwm_task.hpp \
    render_group.hpp \
    rule_drawer.hpp \
    shape_view_params.hpp \
    stylist.hpp \
    text_layout.hpp \
    text_shape.hpp \
    threads_commutator.hpp \
    tile_info.hpp \
    tile_key.hpp \
    tile_tree.hpp \
    tile_tree_builder.hpp \
    tile_utils.hpp \
    user_mark_shapes.hpp \
    user_marks_provider.hpp \
    viewport.hpp \
    visual_params.hpp \
    my_position.hpp \
    user_event_stream.hpp \
