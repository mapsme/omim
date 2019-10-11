cmake_minimum_required(VERSION 3.2)

project(mapswithme C CXX)

include_directories(
  ${OMIM_ROOT}/android/jni
)

set(
  SRC
  # JNI headers
  ../../private.h
  com/mapswithme/core/jni_helper.hpp
  com/mapswithme/core/logging.hpp
  com/mapswithme/core/ScopedEnv.hpp
  com/mapswithme/core/ScopedLocalRef.hpp
  com/mapswithme/maps/discovery/Locals.hpp
  com/mapswithme/maps/Framework.hpp
  com/mapswithme/maps/promo/Promo.hpp
  com/mapswithme/maps/SearchEngine.hpp
  com/mapswithme/opengl/android_gl_utils.hpp
  com/mapswithme/opengl/androidoglcontext.hpp
  com/mapswithme/opengl/androidoglcontextfactory.hpp
  com/mapswithme/opengl/gl3stub.h
  com/mapswithme/platform/GuiThread.hpp
  com/mapswithme/platform/Platform.hpp

  # JNI sources
  com/mapswithme/core/jni_helper.cpp
  com/mapswithme/core/logging.cpp
  com/mapswithme/maps/analytics/ExternalLibrariesMediator.cpp
  com/mapswithme/maps/bookmarks/data/BookmarkManager.cpp
  com/mapswithme/maps/discovery/DiscoveryManager.cpp
  com/mapswithme/maps/discovery/Locals.cpp
  com/mapswithme/maps/DisplayedCategories.cpp
  com/mapswithme/maps/DownloadResourcesLegacyActivity.cpp
  com/mapswithme/maps/editor/OpeningHours.cpp
  com/mapswithme/maps/editor/Editor.cpp
  com/mapswithme/maps/editor/OsmOAuth.cpp
  com/mapswithme/maps/Framework.cpp
  com/mapswithme/maps/LightFramework.cpp
  com/mapswithme/maps/LocationState.cpp
  com/mapswithme/maps/LocationHelper.cpp
  com/mapswithme/maps/MapFragment.cpp
  com/mapswithme/maps/MapManager.cpp
  com/mapswithme/maps/MwmApplication.cpp
  com/mapswithme/maps/metrics/UserActionsLogger.cpp
  com/mapswithme/maps/PrivateVariables.cpp
  com/mapswithme/maps/promo/Promo.cpp
  com/mapswithme/maps/routing/RoutingOptions.cpp
  com/mapswithme/maps/SearchEngine.cpp
  com/mapswithme/maps/SearchRecents.cpp
  com/mapswithme/maps/settings/UnitLocale.cpp
  com/mapswithme/maps/sound/tts.cpp
  com/mapswithme/maps/Sponsored.cpp
  com/mapswithme/maps/taxi/TaxiManager.cpp
  com/mapswithme/maps/TrackRecorder.cpp
  com/mapswithme/maps/TrafficState.cpp
  com/mapswithme/maps/subway/SubwayManager.cpp
  com/mapswithme/maps/ugc/UGC.cpp
  com/mapswithme/maps/UserMarkHelper.cpp
  com/mapswithme/opengl/android_gl_utils.cpp
  com/mapswithme/opengl/androidoglcontext.cpp
  com/mapswithme/opengl/androidoglcontextfactory.cpp
  com/mapswithme/opengl/gl3stub.c
  com/mapswithme/platform/HttpThread.cpp
  com/mapswithme/platform/HttpUserAgent.cpp
  com/mapswithme/platform/GuiThread.cpp
  com/mapswithme/platform/Language.cpp
  com/mapswithme/platform/Localization.cpp
  com/mapswithme/platform/MarketingService.cpp
  com/mapswithme/platform/Platform.cpp
  com/mapswithme/platform/PThreadImpl.cpp
  com/mapswithme/platform/SecureStorage.cpp
  com/mapswithme/platform/SocketImpl.cpp
  com/mapswithme/util/Config.cpp
  com/mapswithme/util/GeoUtils.cpp
  com/mapswithme/util/HttpClient.cpp
  com/mapswithme/util/HttpUploader.cpp
  com/mapswithme/util/Language.cpp
  com/mapswithme/util/LoggerFactory.cpp
  com/mapswithme/util/NetworkPolicy.cpp
  com/mapswithme/util/StringUtils.cpp
  com/mapswithme/util/statistics/PushwooshHelper.cpp
  com/mapswithme/vulkan/android_vulkan_context_factory.cpp
  com/mapswithme/vulkan/android_vulkan_context_factory.hpp
)

omim_add_library(mapswithme SHARED ${SRC})

target_link_libraries(
  ${PROJECT_NAME}
  base
  coding
  drape
  drape_frontend
  editor
  geometry
  indexer
  local_ads
  map
  metrics
  partners_api
  platform
  routing
  search
  storage
)

# 3party libraries or external dependencies
target_link_libraries(
  ${PROJECT_NAME}
  opening_hours
   # Android libs
  log
  android
  EGL
  GLESv2
  atomic
  z
)
