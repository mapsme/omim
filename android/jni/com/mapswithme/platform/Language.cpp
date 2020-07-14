#include "android/jni/com/mapswithme/core/jni_helper.hpp"
#include "android/jni/com/mapswithme/core/ScopedLocalRef.hpp"

#include "platform/locale.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include <string>

/// This function is called from native c++ code
std::string GetAndroidSystemLanguage()
{
  static char const * DEFAULT_LANG = "en";

  JNIEnv * env = jni::GetEnv();
  if (!env)
  {
    LOG(LWARNING, ("Can't get JNIEnv"));
    return DEFAULT_LANG;
  }

  static jclass const languageClass = jni::GetGlobalClassRef(env, "com/mapswithme/util/Language");
  static jmethodID const getDefaultLocaleId = jni::GetStaticMethodID(env, languageClass, "getDefaultLocale", "()Ljava/lang/String;");

  jni::TScopedLocalRef localeRef(env, env->CallStaticObjectMethod(languageClass, getDefaultLocaleId));

  std::string res = jni::ToNativeString(env, (jstring) localeRef.get());
  if (res.empty())
    res = DEFAULT_LANG;

  return res;
}

namespace platform
{
Locale GetCurrentLocale()
{
  JNIEnv * env = jni::GetEnv();
  static jmethodID const getLanguageCodeId = jni::GetStaticMethodID(env, g_utilsClazz, "getLanguageCode",
                                                                    "()Ljava/lang/String;");
  jni::ScopedLocalRef languageCode(env, env->CallStaticObjectMethod(g_utilsClazz, getLanguageCodeId));

  static jmethodID const getCountryCodeId = jni::GetStaticMethodID(env, g_utilsClazz, "getCountryCode",
                                                                   "()Ljava/lang/String;");
  jni::ScopedLocalRef countryCode(env, env->CallStaticObjectMethod(g_utilsClazz, getCountryCodeId));

  static jmethodID const getCurrencyCodeId = jni::GetStaticMethodID(env, g_utilsClazz, "getCurrencyCode",
                                                                    "()Ljava/lang/String;");
  jni::ScopedLocalRef currencyCode(env, env->CallStaticObjectMethod(g_utilsClazz, getCurrencyCodeId));

  return {jni::ToNativeString(env, static_cast<jstring>(languageCode.get())),
          jni::ToNativeString(env, static_cast<jstring>(countryCode.get())),
          currencyCode.get() ? jni::ToNativeString(env, static_cast<jstring>(currencyCode.get())) : ""};
}
}  // namespace platform
