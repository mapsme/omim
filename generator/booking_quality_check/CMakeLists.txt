project(booking_quality_check)

set(
  SRC
  booking_quality_check.cpp
)

omim_add_executable(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  generator
  geometry
  indexer
  map
  platform
  search
  storage
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  gflags
)
