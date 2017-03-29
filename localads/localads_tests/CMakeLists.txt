project(localads_tests)

set(
  SRC
  campaign_serialization_test.cpp
)

omim_add_test(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  localads
  coding
  base
  platform
  stats_client
  ${LIBZ}
)

link_qt5_core(${PROJECT_NAME})
