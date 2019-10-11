project(skin_generator_tool)

set(
  SRC
  generator.cpp
  generator.hpp
  main.cpp
)

if (BUILD_DESIGNER)
  set(SRC MACOSX_BUNDLE ${SRC})
endif()

omim_add_executable(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  geometry
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  gflags
  ${Qt5Gui_LIBRARIES}
  ${Qt5Svg_LIBRARIES}
  ${Qt5Xml_LIBRARIES}
)

link_opengl(${PROJECT_NAME})
link_qt5_core(${PROJECT_NAME})

if (PLATFORM_LINUX)
  omim_link_libraries(
    ${PROJECT_NAME}
    ${CMAKE_DL_LIBS}
  )
endif()
