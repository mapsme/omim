project(openlocationcode)

set(
  SRC
  codearea.cc
  codearea.h
  openlocationcode.cc
  openlocationcode.h
)

add_library(${PROJECT_NAME} ${SRC})

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC
  .
)
