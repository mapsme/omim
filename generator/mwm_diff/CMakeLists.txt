project(mwm_diff)

set(
  SRC
  diff.cpp
  diff.hpp
)

omim_add_library(${PROJECT_NAME} ${SRC})
omim_link_libraries(
  ${PROJECT_NAME}
  coding
)
omim_add_pybindings_subdirectory(pymwm_diff)
omim_add_test_subdirectory(mwm_diff_tests)
