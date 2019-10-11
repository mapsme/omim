project(transit_tests)

set(
  SRC
  transit_graph_test.cpp
  transit_json_parsing_test.cpp
  transit_test.cpp
  transit_tools.hpp
)

omim_add_test(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  transit
)
