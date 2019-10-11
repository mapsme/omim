project(openlr_tests)

set(
  SRC
  decoded_path_test.cpp
)

omim_add_test(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  generator
  indexer
  openlr
  platform
  platform_tests_support
  generator_tests_support
)
