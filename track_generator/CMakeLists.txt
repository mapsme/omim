project(track_generator_tool)

set(
  SRC
  track_generator_tool.cpp
  utils.cpp
  utils.hpp
)

omim_add_executable(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  kml
  platform
  routes_builder
  routing
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  gflags
)

omim_add_pybindings_subdirectory(pytrack_generator)
