project(ugc_tests)

option(UGC_MIGRATION "Build migration file for ugc" OFF)

set(
  SRC
  ratings_tests.cpp
  serdes_binary_tests.cpp
  serdes_tests.cpp
  storage_tests.cpp
  utils.cpp
  utils.hpp
)

if (UGC_MIGRATION)
  set(
    SRC
    ${SRC}
    migration/generate_migration_files.cpp
  )

  message("Generating files for UGC index migration")
endif()

omim_add_test(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  generator
  generator_tests_support
  indexer
  platform
  storage
  ugc
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  jansson
)
