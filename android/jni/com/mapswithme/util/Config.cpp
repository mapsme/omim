#include "../core/jni_helper.hpp"
#include "platform/settings.hpp"


extern "C"
{
  JNIEXPORT jboolean JNICALL
  Java_com_mapswithme_util_Config_nativeGetBoolean(JNIEnv * env, jclass thiz, jstring name, jboolean defaultVal)
  {
    bool val;
    if (Settings::Get(jni::ToNativeString(env, name), val))
      return static_cast<jboolean>(val);

    return defaultVal;
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_util_Config_nativeSetBoolean(JNIEnv * env, jclass thiz, jstring name, jboolean val)
  {
    (void)Settings::Set(jni::ToNativeString(env, name), static_cast<bool>(val));
  }

  JNIEXPORT jint JNICALL
  Java_com_mapswithme_util_Config_nativeGetInt(JNIEnv * env, jclass thiz, jstring name, jint defaultValue)
  {
    int32_t value;
    if (Settings::Get(jni::ToNativeString(env, name), value))
      return static_cast<jint>(value);

    return defaultValue;
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_util_Config_nativeSetInt(JNIEnv * env, jclass thiz, jstring name, jint value)
  {
    (void)Settings::Set(jni::ToNativeString(env, name), static_cast<int32_t>(value));
  }

  JNIEXPORT jlong JNICALL
  Java_com_mapswithme_util_Config_nativeGetLong(JNIEnv * env, jclass thiz, jstring name, jlong defaultValue)
  {
    int64_t value;
    if (Settings::Get(jni::ToNativeString(env, name), value))
      return static_cast<jlong>(value);

    return defaultValue;
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_util_Config_nativeSetLong(JNIEnv * env, jclass thiz, jstring name, jlong value)
  {
    (void)Settings::Set(jni::ToNativeString(env, name), static_cast<int64_t>(value));
  }

  JNIEXPORT jdouble JNICALL
  Java_com_mapswithme_util_Config_nativeGetDouble(JNIEnv * env, jclass thiz, jstring name, jdouble defaultValue)
  {
    double value;
    if (Settings::Get(jni::ToNativeString(env, name), value))
      return static_cast<jdouble>(value);

    return defaultValue;
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_util_Config_nativeSetDouble(JNIEnv * env, jclass thiz, jstring name, jdouble value)
  {
    (void)Settings::Set(jni::ToNativeString(env, name), static_cast<double>(value));
  }

  JNIEXPORT jstring JNICALL
  Java_com_mapswithme_util_Config_nativeGetString(JNIEnv * env, jclass thiz, jstring name, jstring defaultValue)
  {
    string value;
    if (Settings::Get(jni::ToNativeString(env, name), value))
      return jni::ToJavaString(env, value);

    return defaultValue;
  }

    JNIEXPORT void JNICALL
    Java_com_mapswithme_util_Config_nativeSetString(JNIEnv * env, jclass thiz, jstring name, jstring value)
    {
      (void)Settings::Set(jni::ToNativeString(env, name), jni::ToNativeString(env, value));
    }
} // extern "C"
