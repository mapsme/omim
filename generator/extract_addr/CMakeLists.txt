project(extract_addr)

set(
  SRC
  extract_addr.cpp
)

omim_add_executable(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  generator
  geometry
  indexer
  platform
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  jansson
)
