project(tracking_tests)

set(
  SRC
  protocol_test.cpp
  reporter_test.cpp
)

omim_add_test(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  platform
  platform_tests_support
  tracking
)
