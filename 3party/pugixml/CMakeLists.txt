project(pugixml)

set(
  SRC
  src/pugiconfig.hpp
  src/pugixml.cpp
  src/pugixml.hpp
  src/utils.hpp
)

add_library(${PROJECT_NAME} ${SRC})

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC
  src
)
