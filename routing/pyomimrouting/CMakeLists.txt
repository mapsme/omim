project(pyomimrouting)

set(PY_MODULE_PATH ${OMIM_ROOT}/tools/python_tests/routing_memory_tests)

set(
  SRC
  pyomimrouting.cpp
  ${OMIM_ROOT}/routing/routing_integration_tests/routing_test_tools.hpp
  ${OMIM_ROOT}/routing/routing_integration_tests/routing_test_tools.cpp
  ${OMIM_ROOT}/testing/testingmain.cpp
)

add_library(${PROJECT_NAME} SHARED ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  ${Boost_LIBRARIES}
  ${PYTHON_LIBRARIES}
  map
  routing
  search
  storage
  mwm_diff
  indexer
  editor
  traffic
  routing_common
  transit
  kml
  platform
  oauthcpp
  geometry
  coding
  base
  jansson
  protobuf
  bsdiff
  succinct
  stats_client
  pugixml
  opening_hours
  icu
  agg
  ${LIBZ}
)

target_link_libraries(${PROJECT_NAME} Qt5::Core Qt5::Network)

set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES
  PREFIX ""
  SUFFIX ".so"
  LIBRARY_OUTPUT_DIRECTORY ${PY_MODULE_PATH}
)
