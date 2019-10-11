project(editor_tests_support)

set(
  SRC
  helpers.cpp
  helpers.hpp
)

omim_add_library(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  editor
  indexer
)
