project(metrics)

set(
  SRC
  eye.cpp
  eye.hpp
  eye_info.hpp
  eye_serdes.cpp
  eye_serdes.hpp
  eye_storage.cpp
  eye_storage.hpp
)

omim_add_library(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  geometry
  platform
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  jansson
)

omim_add_test_subdirectory(metrics_tests)
omim_add_test_subdirectory(metrics_tests_support)
