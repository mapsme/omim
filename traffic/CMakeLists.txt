project(traffic)

set(
  SRC
  speed_groups.cpp
  speed_groups.hpp
  traffic_cache.cpp
  traffic_cache.hpp
  traffic_info.cpp
  traffic_info.hpp
)

omim_add_library(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  indexer
  platform
  routing_common
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  stats_client
)

omim_add_pybindings_subdirectory(pytraffic)
omim_add_test_subdirectory(traffic_tests)
