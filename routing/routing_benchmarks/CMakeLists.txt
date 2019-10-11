project(routing_benchmarks)

set(
  SRC
  ../routing_integration_tests/routing_test_tools.cpp
  ../routing_integration_tests/routing_test_tools.hpp
  bicycle_routing_tests.cpp
  car_routing_tests.cpp
  helpers.cpp
  helpers.hpp
  pedestrian_routing_tests.cpp
)

omim_add_test(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  geometry
  indexer
  map
  platform
  routing
  routing_common
  storage
  traffic
)

