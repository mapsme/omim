# Indexer library.

TARGET = indexer
TEMPLATE = lib
CONFIG += staticlib warn_on
INCLUDEPATH += ../3party/protobuf/src

ROOT_DIR = ..

include($$ROOT_DIR/common.pri)

SOURCES += \
    classificator.cpp \
    classificator_loader.cpp \
    coding_params.cpp \
    data_factory.cpp \
    data_header.cpp \
    drawing_rule_def.cpp \
    drawing_rules.cpp \
    drules_selector.cpp \
    drules_selector_parser.cpp \
    feature.cpp \
    feature_algo.cpp \
    feature_covering.cpp \
    feature_data.cpp \
    feature_decl.cpp \
    feature_impl.cpp \
    feature_loader.cpp \
    feature_loader_base.cpp \
    feature_meta.cpp \
    feature_utils.cpp \
    feature_visibility.cpp \
    features_offsets_table.cpp \
    features_vector.cpp \
    ftypes_matcher.cpp \
    geometry_coding.cpp \
    geometry_serialization.cpp \
    index.cpp \
    index_builder.cpp \
    map_style.cpp \
    map_style_reader.cpp \
    mwm_set.cpp \
    old/feature_loader_101.cpp \
    osm_editor.cpp \
    point_to_int64.cpp \
    rank_table.cpp \
    scales.cpp \
    types_mapping.cpp \
    types_skipper.cpp \

HEADERS += \
    cell_coverer.hpp \
    cell_id.hpp \
    classificator.hpp \
    classificator_loader.hpp \
    coding_params.hpp \
    data_factory.hpp \
    data_header.hpp \
    drawing_rule_def.hpp \
    drawing_rules.hpp \
    drules_include.hpp \
    drules_selector.cpp \
    drules_selector_parser.cpp \
    feature.hpp \
    feature_algo.hpp \
    feature_covering.hpp \
    feature_data.hpp \
    feature_decl.hpp \
    feature_impl.hpp \
    feature_loader.hpp \
    feature_loader_base.hpp \
    feature_meta.hpp \
    feature_processor.hpp \
    feature_utils.hpp \
    feature_visibility.hpp \
    features_offsets_table.hpp \
    features_vector.hpp \
    ftypes_matcher.hpp \
    geometry_coding.hpp \
    geometry_serialization.hpp \
    index.hpp \
    index_builder.hpp \
    interval_index.hpp \
    interval_index_builder.hpp \
    interval_index_iface.hpp \
    map_style.hpp \
    map_style_reader.hpp \
    mwm_set.hpp \
    old/feature_loader_101.hpp \
    old/interval_index_101.hpp \
    osm_editor.hpp \
    point_to_int64.hpp \
    rank_table.hpp \
    scale_index.hpp \
    scale_index_builder.hpp \
    scales.hpp \
    succinct_trie_builder.hpp \
    succinct_trie_reader.hpp \
    tesselator_decl.hpp \
    tree_structure.hpp \
    trie.hpp \
    trie_builder.hpp \
    trie_reader.hpp \     
    types_mapping.hpp \
    types_skipper.hpp \
    unique_index.hpp \

OTHER_FILES += drules_struct.proto

SOURCES += drules_struct.pb.cc
HEADERS += drules_struct.pb.h
