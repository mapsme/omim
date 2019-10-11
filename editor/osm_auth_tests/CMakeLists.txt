project(osm_auth_tests)

set(
  SRC
  osm_auth_tests.cpp
  server_api_test.cpp
)

omim_add_test(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  editor
  geometry
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  pugixml
)
