project(partners_api_tests)

add_definitions(-DOMIM_UNIT_TEST_WITH_QT_EVENT_LOOP)

set(
  SRC
  ads_engine_tests.cpp
  booking_tests.cpp
  facebook_tests.cpp
  google_tests.cpp
  maxim_tests.cpp
  megafon_countries_tests.cpp
  mopub_tests.cpp
  promo_tests.cpp
  rb_tests.cpp
  rutaxi_tests.cpp
  taxi_engine_tests.cpp
  taxi_places_tests.cpp
  uber_tests.cpp
  yandex_tests.cpp
)

omim_add_test(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  base
  geometry
  indexer
  metrics
  metrics_tests_support
  partners_api
  platform
  platform_tests_support
  platform_tests_support
)

# 3party libraries or external dependencies
omim_link_libraries(
  ${PROJECT_NAME}
  jansson
)
