project(openlr_stat)

set(
  SRC
  openlr_stat.cpp
)

omim_add_executable(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  indexer
  openlr
  platform
  routing
  storage
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  gflags
  pugixml
)
