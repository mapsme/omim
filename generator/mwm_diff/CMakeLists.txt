project(mwm_diff)

set(
  SRC
  diff.cpp
  diff.hpp
)

omim_add_library(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  bsdiff
)

omim_add_pybindings_subdirectory(pymwm_diff)
omim_add_test_subdirectory(mwm_diff_tests)
