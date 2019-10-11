project(tracking)

set(
  SRC
  connection.cpp
  connection.hpp
  protocol.cpp
  protocol.hpp
  reporter.cpp
  reporter.hpp
)

omim_add_library(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  platform
  traffic
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  stats_client
)

omim_add_pybindings_subdirectory(pytracking)
omim_add_test_subdirectory(tracking_tests)
