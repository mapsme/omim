TARGET = drape
TEMPLATE = lib
CONFIG += staticlib warn_on

ROOT_DIR = ..
SHADER_COMPILE_ARGS = $$PWD/shaders shader_index.txt shader_def
include($$ROOT_DIR/common.pri)

DRAPE_DIR = .
include($$DRAPE_DIR/drape_common.pri)

SOURCES += glfunctions.cpp

OTHER_FILES += \
    shaders/area_vertex_shader.vsh \
    shaders/button_fragment_shader.fsh \
    shaders/button_vertex_shader.vsh \
    shaders/circle_shader.fsh \
    shaders/circle_shader.vsh \
    shaders/compass_vertex_shader.vsh \
    shaders/dashed_fragment_shader.fsh \
    shaders/dashed_vertex_shader.vsh \
    shaders/debug_rect_fragment_shader.fsh \
    shaders/debug_rect_vertex_shader.vsh \
    shaders/line_fragment_shader.fsh \
    shaders/line_vertex_shader.vsh \
    shaders/my_position_shader.vsh \
    shaders/path_symbol_vertex_shader.vsh \
    shaders/position_accuracy_shader.vsh \
    shaders/solid_color_fragment_shader.fsh \
    shaders/route_arrow_fragment_shader.fsh \
    shaders/route_fragment_shader.fsh \
    shaders/route_vertex_shader.vsh \
    shaders/ruler_vertex_shader.vsh \
    shaders/shader_index.txt \
    shaders/text_fragment_shader.fsh \
    shaders/texturing_fragment_shader.fsh \
    shaders/texturing_vertex_shader.vsh \
    shaders/user_mark.vsh \
    shaders/text_outlined_vertex_shader.vsh \
    shaders/text_vertex_shader.vsh \
    shaders/trackpoint_vertex_shader.vsh \
    shaders/trackpoint_fragment_shader.fsh \
