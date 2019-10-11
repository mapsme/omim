project(search_quality_tests)

set(SRC sample_test.cpp)

omim_add_test(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  search
  search_quality
)
