# Build file for MAPS.ME project
#
# Possible options:
#   gtool: build only generator_tool
#   map_designer: enable designer-related flags
#   no-tests: do not build tests for desktop
#   drape: include drape libraries
#   iphone / tizen / android: build an app (implies no-tests)

lessThan(QT_MAJOR_VERSION, 5) {
  error("You need Qt 5 to build this project. You have Qt $$QT_VERSION")
}

cache()

TEMPLATE = subdirs
CONFIG += ordered

HEADERS += defines.hpp

!iphone*:!tizen*:!android* {
  CONFIG *= desktop
}

SUBDIRS = 3party base geometry coding

SUBDIRS += platform
SUBDIRS += stats
SUBDIRS += indexer
SUBDIRS += routing
SUBDIRS += storage

# Integration tests dependencies for gtool
CONFIG(gtool):!CONFIG(no-tests) {
  SUBDIRS += search
  SUBDIRS += map
  SUBDIRS += routing/routing_integration_tests
}

CONFIG(desktop) {
  SUBDIRS += generator generator/generator_tool
}

!CONFIG(gtool) {
  SUBDIRS *= drape
  SUBDIRS *= drape_frontend
  SUBDIRS *= search
  SUBDIRS *= map

  CONFIG(desktop) {
    SUBDIRS += qt
    SUBDIRS += drape_head
  }

  CONFIG(map_designer):CONFIG(desktop) {
    SUBDIRS += skin_generator
  }

  CONFIG(desktop):!CONFIG(no-tests) {
    SUBDIRS += base/base_tests
    SUBDIRS += coding/coding_tests
    SUBDIRS += platform/platform_tests_support
    SUBDIRS += geometry/geometry_tests
    SUBDIRS += platform/platform_tests
    SUBDIRS += qt_tstfrm
    SUBDIRS += storage/storage_tests
    SUBDIRS += search/search_tests
    SUBDIRS += map/map_tests map/benchmark_tool map/mwm_tests
    SUBDIRS += routing/routing_integration_tests
    SUBDIRS += routing/routing_tests
    SUBDIRS += generator/generator_tests
    SUBDIRS += indexer/indexer_tests
    SUBDIRS += pedestrian_routing_benchmarks
    SUBDIRS += search/search_integration_tests
    SUBDIRS += drape/drape_tests
    SUBDIRS += drape_frontend/drape_frontend_tests
  } # !no-tests
} # !gtool
