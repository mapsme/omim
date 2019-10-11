project(samples_generation_tool)

set(SRC samples_generation_tool.cpp)

omim_add_executable(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  geometry
  indexer
  platform
  search
  search_quality
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  gflags
)
