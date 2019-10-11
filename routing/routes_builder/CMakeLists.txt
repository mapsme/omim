project(routes_builder)

set(
  SRC
  data_source_storage.cpp
  data_source_storage.hpp
  routes_builder.cpp
  routes_builder.hpp
)

omim_add_library(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  geometry
  indexer
  platform
  routing
  routing_common
  storage
  traffic
)

add_subdirectory(routes_builder_tool)
