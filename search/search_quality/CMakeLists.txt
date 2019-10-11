project(search_quality)

set(SRC
  helpers.cpp
  helpers.hpp
  helpers_json.cpp
  helpers_json.hpp
  matcher.cpp
  matcher.hpp
  sample.cpp
  sample.hpp
)

omim_add_library(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  geometry
  indexer
  platform
  search
  search_tests_support
  storage
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  jansson
)

if (NOT SKIP_DESKTOP)
  add_subdirectory(assessment_tool)
endif()

add_subdirectory(booking_dataset_generator)
add_subdirectory(features_collector_tool)
add_subdirectory(samples_generation_tool)
add_subdirectory(search_quality_tool)
omim_add_test_subdirectory(search_quality_tests)
