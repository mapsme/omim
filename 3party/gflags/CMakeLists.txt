project(gflags)

set(
  SRC
  src/config.h
  src/gflags.cc
  src/gflags/gflags.h
  src/gflags/gflags_completions.h
  src/gflags_completions.cc
  src/gflags_reporting.cc
)

add_library(${PROJECT_NAME} ${SRC})

target_include_directories(
  ${PROJECT_NAME} BEFORE
  PUBLIC
  src
)
