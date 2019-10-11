project(kml_tests)

set(
  SRC
  serdes_tests.cpp
  tests_data.hpp
)

omim_add_test(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  indexer
  kml
)
