project(style_tests)

set(
  SRC
  classificator_tests.cpp
  dashes_test.cpp
  helpers.hpp
  style_symbols_consistency_test.cpp
)

if (BUILD_DESIGNER)
  set(SRC MACOSX_BUNDLE ${SRC})
endif()

omim_add_test(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  indexer
  map
  platform
)
