project(map_integration_tests)

set(
  SRC
  interactive_search_test.cpp
)

omim_add_test(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  editor_tests_support
  generator_tests_support
  map
  search
  search_tests_support
)
