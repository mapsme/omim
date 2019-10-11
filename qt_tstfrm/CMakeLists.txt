project(qt_tstfrm)

set(
  SRC
  test_main_loop.hpp
  test_main_loop.cpp
)

omim_add_library(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
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
