project(assessment_tool)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

set(
  SRC
  assessment_tool.cpp
  context.cpp
  context.hpp
  edits.cpp
  edits.hpp
  feature_info_dialog.cpp
  feature_info_dialog.hpp
  helpers.cpp
  helpers.hpp
  main_model.cpp
  main_model.hpp
  main_view.cpp
  main_view.hpp
  model.hpp
  result_view.cpp
  result_view.hpp
  results_view.cpp
  results_view.hpp
  sample_view.cpp
  sample_view.hpp
  samples_view.cpp
  samples_view.hpp
  search_request_runner.cpp
  search_request_runner.hpp
  view.hpp
)

omim_add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  geometry
  indexer
  kml
  map
  platform
  qt_common
  search
  search_quality
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  gflags
  ${Qt5Gui_LIBRARIES}
  ${Qt5Widgets_LIBRARIES}
)

link_opengl(${PROJECT_NAME})
link_qt5_core(${PROJECT_NAME})

if (PLATFORM_LINUX)
  # 3party libraries or external dependencies
  omim_link_libraries(
    ${PROJECT_NAME}
    ${CMAKE_DL_LIBS}
  )
endif()

if (PLATFORM_MAC)
  set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/Info.plist
  )
endif()
