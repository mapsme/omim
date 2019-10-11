project(metrics_tests_support)

set(
  SRC
  eye_for_testing.cpp
  eye_for_testing.hpp
)

omim_add_library(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  metrics
  platform
)
