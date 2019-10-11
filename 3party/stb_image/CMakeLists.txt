project(stb_image)

set(
  SRC
  stb_image.cpp
  stb_image.h
)

add_library(${PROJECT_NAME} ${SRC})

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC
  .
)
