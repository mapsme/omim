project(generator_integration_tests)

set(
  SRC
  features_tests.cpp
  helpers.cpp
  helpers.hpp
)

omim_add_test(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  generator
  indexer
  platform
)
