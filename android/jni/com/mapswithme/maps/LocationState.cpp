#include "Framework.hpp"

#include "com/mapswithme/core/jni_helper.hpp"

#include "com/mapswithme/platform/Platform.hpp"

extern "C"
{

static void LocationStateModeChanged(location::EMyPositionMode mode,
                                     std::shared_ptr<jobject> const & listener)
{
  g_framework->OnMyPositionModeChanged(mode);

  JNIEnv * env = jni::GetEnv();
  env->CallVoidMethod(*listener, jni::GetMethodID(env, *listener.get(),
                      "onMyPositionModeChanged", "(I)V"), static_cast<jint>(mode));
}

static void LocationPendingTimeout(std::shared_ptr<jobject> const & listener)
{
  JNIEnv * env = jni::GetEnv();
  env->CallVoidMethod(*listener, jni::GetMethodID(env, *listener.get(),
                      "onLocationPendingTimeout", "()V"));
}

//  public static void nativeSwitchToNextMode();
JNIEXPORT void JNICALL
Java_com_mapswithme_maps_location_LocationState_nativeSwitchToNextMode(JNIEnv * env, jclass clazz)
{
  g_framework->SwitchMyPositionNextMode();
}

// private static int nativeGetMode();
JNIEXPORT jint JNICALL
Java_com_mapswithme_maps_location_LocationState_nativeGetMode(JNIEnv * env, jclass clazz)
{
  return g_framework->GetMyPositionMode();
}

//  public static void nativeSetListener(ModeChangeListener listener);
JNIEXPORT void JNICALL
Java_com_mapswithme_maps_location_LocationState_nativeSetListener(JNIEnv * env, jclass clazz,
                                                                  jobject listener)
{
  g_framework->SetMyPositionModeListener(std::bind(&LocationStateModeChanged, std::placeholders::_1,
                                                   jni::make_global_ref(listener)));
}

//  public static void nativeRemoveListener();
JNIEXPORT void JNICALL
Java_com_mapswithme_maps_location_LocationState_nativeRemoveListener(JNIEnv * env, jclass clazz)
{
  g_framework->SetMyPositionModeListener(location::TMyPositionModeChanged());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_location_LocationState_nativeSetLocationPendingTimeoutListener(
  JNIEnv * env, jclass clazz, jobject listener)
{
  g_framework->NativeFramework()->SetMyPositionPendingTimeoutListener(
    std::bind(&LocationPendingTimeout, jni::make_global_ref(listener)));
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_location_LocationState_nativeRemoveLocationPendingTimeoutListener(
  JNIEnv * env, jclass)
{
  g_framework->NativeFramework()->SetMyPositionPendingTimeoutListener(nullptr);
}
} // extern "C"
