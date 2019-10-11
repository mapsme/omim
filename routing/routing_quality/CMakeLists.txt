project(routing_quality)

set(
  SRC
  waypoints.cpp
  waypoints.hpp
)

omim_add_library(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  geometry
  routes_builder
  routing
)

add_subdirectory(api)
add_subdirectory(routing_quality_tool)

omim_add_test_subdirectory(routing_quality_tests)
