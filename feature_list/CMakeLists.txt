project(feature_list_generator)

set(SRC feature_list.cpp)

omim_add_executable(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  generator
  geometry
  indexer
  platform
  search
  search_quality
  storage
)
