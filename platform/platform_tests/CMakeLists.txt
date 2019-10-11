project(platform_tests)

add_definitions(-DOMIM_UNIT_TEST_WITH_QT_EVENT_LOOP)

set(
  SRC
  apk_test.cpp
  country_file_tests.cpp
  downloader_tests/downloader_test.cpp
  get_text_by_id_tests.cpp
  jansson_test.cpp
  language_test.cpp
  local_country_file_tests.cpp
  location_test.cpp
  measurement_tests.cpp
  mwm_version_test.cpp
  platform_test.cpp
)

omim_add_test(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  platform
  platform_tests_support
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  jansson
)

link_qt5_core(${PROJECT_NAME})

if (PLATFORM_LINUX)
  # 3party libraries or external dependencies
  omim_link_libraries(
    ${PROJECT_NAME}
    ${CMAKE_DL_LIBS}
  )
endif()
