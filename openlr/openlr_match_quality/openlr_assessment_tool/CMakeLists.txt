project(openlr_assessment_tool)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

set(
  SRC
  main.cpp
  mainwindow.cpp
  mainwindow.hpp
  map_widget.cpp
  map_widget.hpp
  points_controller_delegate_base.hpp
  segment_correspondence.cpp
  segment_correspondence.hpp
  traffic_drawer_delegate_base.hpp
  traffic_mode.cpp
  traffic_mode.hpp
  traffic_panel.cpp
  traffic_panel.hpp
  trafficmodeinitdlg.cpp
  trafficmodeinitdlg.h
)

omim_add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  drape_frontend
  geometry
  indexer
  map
  openlr
  platform
  qt_common
  routing
  routing_common
  storage
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  gflags
  ${Qt5Widgets_LIBRARIES}
)

link_opengl(${PROJECT_NAME})
link_qt5_core(${PROJECT_NAME})

if (PLATFORM_LINUX)
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
