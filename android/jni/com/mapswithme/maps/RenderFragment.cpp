#include "Framework.hpp"

#include "../core/jni_helper.hpp"

extern "C"
{

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_RenderFragment_createEngine(JNIEnv * env, jobject thiz, jobject surface, jint destiny)
{
  return static_cast<jboolean>(g_framework->CreateDrapeEngine(env, surface, static_cast<int>(destiny)));
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_RenderFragment_surfaceResized(JNIEnv * env, jobject thiz, jint w, jint h)
{
  g_framework->Resize(static_cast<int>(w), static_cast<int>(h));
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_RenderFragment_destroyEngine(JNIEnv * env, jobject thiz)
{
  g_framework->DeleteDrapeEngine();
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_RenderFragment_isEngineCreated(JNIEnv * env, jobject thiz)
{
  return static_cast<jboolean>(g_framework->IsDrapeEngineCreated());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_RenderFragment_detachSurface(JNIEnv * env, jobject thiz)
{
  g_framework->DetachSurface();
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_RenderFragment_attachSurface(JNIEnv * env, jobject thiz, jobject surface)
{
  g_framework->AttachSurface(env, surface);
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_RenderFragment_onTouch(JNIEnv * env, jobject thiz, jint action, jboolean hasFirst, jboolean hasSecond,
                                                 jfloat x1, jfloat y1, jfloat x2, jfloat y2)
{
  int mask = 0;
  if (hasFirst == JNI_TRUE)
    mask |= 0x1;
  if (hasSecond == JNI_TRUE)
    mask |= 0x2;

  g_framework->Touch(static_cast<int>(action), mask,
                     static_cast<double>(x1),
                     static_cast<double>(y1),
                     static_cast<double>(x2),
                     static_cast<double>(y2));

  return JNI_TRUE;
}

}
