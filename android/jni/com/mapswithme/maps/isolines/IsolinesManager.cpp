#include <jni.h>
#include "com/mapswithme/maps/Framework.hpp"
#include "com/mapswithme/core/jni_helper.hpp"
#include "com/mapswithme/platform/Platform.hpp"

using namespace std::placeholders;

extern "C"
{
static void IsolinesStateChanged(IsolinesManager::IsolinesState state,
                                 std::shared_ptr<jobject> const & listener)
{
  LOG(LINFO, (static_cast<int>(state)));
  JNIEnv * env = jni::GetEnv();
  env->CallVoidMethod(*listener,
                      jni::GetMethodID(env, *listener, "onStateChanged", "(I)V"),
                      static_cast<jint>(state));
}


JNIEXPORT void JNICALL
Java_com_mapswithme_maps_maplayer_isolines_IsolinesManager_nativeAddListener(JNIEnv *env, jclass clazz, jobject listener)
{
  CHECK(g_framework, ("Framework isn't created yet!"));
  g_framework->SetIsolinesListener(std::bind(&IsolinesStateChanged,
                                   std::placeholders::_1,
                                   jni::make_global_ref(listener)));
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_maplayer_isolines_IsolinesManager_nativeRemoveListener(JNIEnv * env, jclass clazz)
{
  CHECK(g_framework, ("Framework isn't created yet!"));
  g_framework->SetIsolinesListener(nullptr);
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_maplayer_isolines_IsolinesManager_nativeShouldShowNotification(JNIEnv *env,
        jclass clazz)
{
  CHECK(g_framework, ("Framework isn't created yet!"));
  auto const &manager = g_framework->NativeFramework()->GetIsolinesManager();
  auto const visible = manager.IsVisible();
  auto const enabled = manager.GetState() == IsolinesManager::IsolinesState::Enabled;
  return static_cast<jboolean>(!visible && enabled);
}
}
