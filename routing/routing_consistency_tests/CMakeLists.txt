# This subproject implements routing consistency tests.
# This tests are launched on the whole world dataset.

project(routing_consistency_tests)

include_directories(${OMIM_ROOT}/3party/gflags/src)

set(
  SRC
  ../routing_integration_tests/routing_test_tools.cpp
  routing_consistency_tests.cpp
)

omim_add_test(
  ${PROJECT_NAME}
  LABELS Maps
  SOURCES ${SRC}
  CUSTOM_TEST
)

omim_link_libraries(
  ${PROJECT_NAME}
  map
  routing
  traffic
  routing_common
  transit
  search
  storage
  mwm_diff
  kml
  editor
  indexer
  platform
  geometry
  oauthcpp
  opening_hours
  coding
  base
  jansson
  protobuf
  bsdiff
  succinct
  stats_client
  generator
  gflags
  pugixml
  icu
  agg
  ${Qt5Widgets_LIBRARIES}
  ${LIBZ}
)

link_qt5_core(${PROJECT_NAME})
link_qt5_network(${PROJECT_NAME})
