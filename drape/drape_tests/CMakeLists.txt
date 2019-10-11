project(drape_tests)

add_definitions(
  -DOGL_TEST_ENABLED
  -DGTEST_DONT_DEFINE_TEST
)

set(
  SRC
  attribute_provides_tests.cpp
  batcher_tests.cpp
  bidi_tests.cpp
  bingind_info_tests.cpp
  buffer_tests.cpp
  dummy_texture.hpp
  failure_reporter.cpp
  font_texture_tests.cpp
  gl_functions.cpp
  gl_mock_functions.cpp
  gl_mock_functions.hpp
  glyph_mng_tests.cpp
  glyph_packer_test.cpp
  img.cpp
  img.hpp
  memory_comparer.hpp
  object_pool_tests.cpp
  pointers_tests.cpp
  static_texture_tests.cpp
  stipple_pen_tests.cpp
  texture_of_colors_tests.cpp
  testing_graphics_context.hpp
  testingmain.cpp
  uniform_value_tests.cpp
)

omim_add_executable(${PROJECT_NAME} ${DRAPE_COMMON_SRC} ${VULKAN_SRC} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  drape
  indexer
  platform
  qt_tstfrm
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  gmock
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
