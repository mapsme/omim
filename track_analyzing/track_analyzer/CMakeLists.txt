project(track_analyzer)

set(
  SRC
  cmd_cpp_track.cpp
  cmd_gpx.cpp
  cmd_match.cpp
  cmd_table.cpp
  cmd_track.cpp
  cmd_tracks.cpp
  cmd_unmatched_tracks.cpp
  crossroad_checker.cpp
  crossroad_checker.hpp
  track_analyzer.cpp
  utils.cpp
  utils.hpp
)

omim_add_executable(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  geometry
  indexer
  platform
  routing
  routing_common
  storage
  track_analyzing
  traffic
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  gflags
)
