#include "Framework.hpp"

#include "map/gps_tracking.hpp"

#include "std/chrono.hpp"

namespace
{

::Framework * frm()
{
  // TODO (trashkalmar): Temp solution until the GPS tracker is uncoupled from the framework.
  return (g_framework ? g_framework->NativeFramework() : nullptr);
}

} // namespace

extern "C"
{
  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_location_TrackRecorder_nativeSetEnabled(JNIEnv * env, jclass clazz, jboolean enable)
  {
    // TODO (trashkalmar): Temp solution until the GPS tracker is uncoupled from the framework.

    ::Framework * const framework = frm();
    if (framework)
    {
      framework->EnableGpsTracking(enable);
      return;
    }

    GpsTracking::Instance().SetEnabled(enable);
  }

  JNIEXPORT jboolean JNICALL
  Java_com_mapswithme_maps_location_TrackRecorder_nativeIsEnabled(JNIEnv * env, jclass clazz)
  {
    // TODO (trashkalmar): Temp solution until the GPS tracker is uncoupled from the framework.

    ::Framework * const framework = frm();
    if (framework)
      return framework->IsGpsTrackingEnabled();

    return GpsTracking::Instance().IsEnabled();
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_location_TrackRecorder_nativeSetDuration(JNIEnv * env, jclass clazz, jint durationHours)
  {
    frm()->SetGpsTrackingDuration(hours(durationHours));
  }

  JNIEXPORT jint JNICALL
  Java_com_mapswithme_maps_location_TrackRecorder_nativeGetDuration(JNIEnv * env, jclass clazz)
  {
    // TODO (trashkalmar): Temp solution until the GPS tracker is uncoupled from the framework.

    ::Framework * const framework = frm();
    if (framework)
      return framework->GetGpsTrackingDuration().count();

    return GpsTracking::Instance()::GetDuration().count();
  }
}
