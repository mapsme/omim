project(qt_common)

QT5_ADD_RESOURCES(RESOURCES res/resources_common.qrc)

set_property(SOURCE qrc_resources_common.cpp PROPERTY SKIP_AUTOGEN ON)

set(
  SRC
  helpers.cpp
  helpers.hpp
  map_widget.cpp
  map_widget.hpp
  proxy_style.cpp
  proxy_style.hpp
  qtoglcontext.cpp
  qtoglcontext.hpp
  qtoglcontextfactory.cpp
  qtoglcontextfactory.hpp
  scale_slider.cpp
  scale_slider.hpp
  spinner.cpp
  spinner.hpp
)

omim_add_library(${PROJECT_NAME} ${SRC} ${RESOURCES})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  drape
  drape_frontend
  geometry
  indexer
  kml
  map
  platform
  search
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
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
