project(ugc)

set(
  SRC
  api.cpp
  api.hpp
  serdes.hpp
  serdes_binary.cpp
  serdes_binary.hpp
  serdes_json.hpp
  storage.cpp
  storage.hpp
  types.hpp
)

add_library(${PROJECT_NAME} ${SRC})
omim_add_test_subdirectory(ugc_tests)
