project(gen)

set(
  SRC
  common.cpp
  common.hpp
  preprocess.cpp
  gen.cpp
)

if (BUILD_DESIGNER)
  set(SRC MACOSX_BUNDLE ${SRC})
endif()

omim_add_executable(${PROJECT_NAME} gen.cpp)
omim_add_executable("${PROJECT_NAME}-preprocess" preprocess.cpp)

omim_link_libraries(
  ${PROJECT_NAME}
  generator
  routing
  traffic
  routing_common
  transit
  ugc
  search
  storage
  editor
  indexer
  platform
  geometry
  coding
  base
  opening_hours
  freetype
  expat
  icu
  jansson
  protobuf
  mwm_diff
  bsdiff
  stats_client
  minizip
  succinct
  pugixml
  tess2
  gflags
  oauthcpp
  sqlite3
  boost_program_options
  boost_system
  boost_filesystem
  ${LIBZ}
)
