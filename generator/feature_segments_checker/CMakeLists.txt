project(feature_segments_checker)

set(
  SRC
  feature_segments_checker.cpp
)

omim_add_executable(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  generator
  geometry
  indexer
  platform
  routing_common
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  gflags
)
