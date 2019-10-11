project(map_tests)

add_definitions("-DOMIM_UNIT_TEST_WITH_QT_EVENT_LOOP")

set(
  SRC
  address_tests.cpp
  booking_availability_cache_test.cpp
  booking_filter_test.cpp
  bookmarks_test.cpp
  chart_generator_tests.cpp
  check_mwms.cpp
  cloud_tests.cpp
  countries_names_tests.cpp
  extrapolator_tests.cpp
  feature_getters_tests.cpp
  framework_light_tests.cpp
  ge0_parser_tests.cpp
  geourl_test.cpp
  gps_track_collection_test.cpp
  gps_track_storage_test.cpp
  gps_track_test.cpp
  kmz_unarchive_test.cpp
  mwm_url_tests.cpp
  notification_tests.cpp
  power_manager_tests.cpp
  search_api_tests.cpp
  tips_tests.cpp
  transliteration_test.cpp
  working_time_tests.cpp
)

omim_add_test(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  drape_frontend
  generator
  generator_tests_support
  geometry
  indexer
  map
  metrics
  metrics_tests_support
  partners_api
  platform
  platform_tests_support
  search
  search_tests_support
  storage
)
