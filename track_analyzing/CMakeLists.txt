project(track_analyzing)

set(
  SRC
  exceptions.hpp
  log_parser.cpp
  log_parser.hpp
  serialization.hpp
  track.cpp
  track.hpp
  track_matcher.cpp
  track_matcher.hpp
  utils.cpp
  utils.hpp
)

omim_add_library(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  generator
  geometry
  indexer
  platform
  routing
  routing_common
  storage
)

add_subdirectory(track_analyzer)
