project(storage_tests)

add_definitions("-DOMIM_UNIT_TEST_WITH_QT_EVENT_LOOP")

set(
  SRC
  country_info_getter_tests.cpp
  country_name_getter_tests.cpp
  fake_map_files_downloader.cpp
  fake_map_files_downloader.hpp
  helpers.cpp
  helpers.hpp
  simple_tree_test.cpp
  storage_tests.cpp
  task_runner.cpp
  task_runner.hpp
  test_map_files_downloader.cpp
  test_map_files_downloader.hpp
)

omim_add_test(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  generator
  generator_tests_support
  geometry
  indexer
  platform
  platform_tests_support
  storage
)
