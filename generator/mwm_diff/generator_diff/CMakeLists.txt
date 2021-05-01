project(generator_diff)

include_directories(${OMIM_ROOT}/3party/gflags/src)

set(SRC generator_diff.cpp)

omim_add_executable(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  mwm_diff
  bsdiff
  platform
  coding
  base
  stats_client
  gflags
  ${LIBZ}
)

link_qt5_core(${PROJECT_NAME})
