TARGET = drape
TEMPLATE = lib
CONFIG += staticlib warn_on

DEPENDENCIES = base

ROOT_DIR = ..
SHADER_COMPILE_ARGS = $$PWD/shaders shader_index.txt shader_def
include($$ROOT_DIR/common.pri)

DRAPE_DIR = .
include($$DRAPE_DIR/drape_common.pri)

SOURCES += glfunctions.cpp

OTHER_FILES += \
    shaders/texturing_vertex_shader.vsh \
    shaders/texturing_fragment_shader.fsh \
    shaders/shader_index.txt \
    shaders/line_vertex_shader.vsh \
    shaders/line_fragment_shader.fsh \
    shaders/text_fragment_shader.fsh \
    shaders/text_vertex_shader.vsh \
    shaders/compass_vertex_shader.vsh \
    shaders/ruler_vertex_shader.vsh \
