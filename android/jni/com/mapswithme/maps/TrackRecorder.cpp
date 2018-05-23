#include "Framework.hpp"

#include "map/gps_tracker.hpp"

#include <chrono>

namespace
{

::Framework * frm()
{
  return (g_framework ? g_framework->NativeFramework() : nullptr);
}

}  // namespace

extern "C"
{
  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_location_TrackRecorder_nativeSetEnabled(JNIEnv * env, jclass clazz, jboolean enable)
  {
    GpsTracker::Instance().SetEnabled(enable);
    Framework * const f = frm();
    if (f == nullptr)
      return;
    if (enable)
      f->ConnectToGpsTracker();
    else
      f->DisconnectFromGpsTracker();
  }

  JNIEXPORT jboolean JNICALL
  Java_com_mapswithme_maps_location_TrackRecorder_nativeIsEnabled(JNIEnv * env, jclass clazz)
  {
    return GpsTracker::Instance().IsEnabled();
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_location_TrackRecorder_nativeSetDuration(JNIEnv * env, jclass clazz, jint durationHours)
  {
    GpsTracker::Instance().SetDuration(std::chrono::hours(durationHours));
  }

  JNIEXPORT jint JNICALL
  Java_com_mapswithme_maps_location_TrackRecorder_nativeGetDuration(JNIEnv * env, jclass clazz)
  {
    return GpsTracker::Instance().GetDuration().count();
  }
}
