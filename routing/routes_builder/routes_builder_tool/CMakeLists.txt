project(routes_builder_tool)

set(
  SRC
  routes_builder_tool.cpp
  utils.cpp
  utils.hpp
)

omim_add_executable(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  geometry
  platform
  routes_builder
  routing
  routing_api
  routing_quality
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  gflags
)
