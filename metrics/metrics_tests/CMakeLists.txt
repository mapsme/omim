project(metrics_tests)

set(
  SRC
  eye_tests.cpp
)

omim_add_test(${PROJECT_NAME} ${SRC})
omim_link_libraries(
  ${PROJECT_NAME}
  coding
  metrics
  metrics_tests_support
)
