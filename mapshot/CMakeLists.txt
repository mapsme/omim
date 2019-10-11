project(mapshot_tool)

set(
  SRC
  mapshot.cpp
)

omim_add_executable(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  drape_frontend
  geometry
  map
  software_renderer
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  gflags
)
