#include <jni.h>

#include "com/mapswithme/core/jni_helper.hpp"

#include "base/logging.hpp"
#include "editor/osm_auth.hpp"

namespace
{
using namespace osm;
using namespace jni;

enum class AuthType
{
  IZ_SERVER = 0,
  DEV_SERVER,
  PRODUCTION,
  SERVER
};

OsmOAuth const authForType(int type)
{
  switch (static_cast<AuthType>(type))
  {
  case AuthType::IZ_SERVER:
    return OsmOAuth::IZServerAuth();
  case AuthType::DEV_SERVER:
    return OsmOAuth::DevServerAuth();
  case AuthType::PRODUCTION:
    return OsmOAuth::ProductionServerAuth();
  case AuthType::SERVER:
      return OsmOAuth::ServerAuth();
  }
}

jobjectArray ToStringArray(JNIEnv * env, TKeySecret const & secret)
{
  jobjectArray resultArray = env->NewObjectArray(2, GetStringClass(env), nullptr);
  env->SetObjectArrayElement(resultArray, 0, ToJavaString(env, secret.first));
  env->SetObjectArrayElement(resultArray, 1, ToJavaString(env, secret.second));
  return resultArray;
}

jobjectArray ToStringArray(JNIEnv * env, string const & secret, string const & token, string const & url)
{
  jobjectArray resultArray = env->NewObjectArray(3, GetStringClass(env), nullptr);
  env->SetObjectArrayElement(resultArray, 0, ToJavaString(env, token));
  env->SetObjectArrayElement(resultArray, 1, ToJavaString(env, token));
  env->SetObjectArrayElement(resultArray, 2, ToJavaString(env, url));
  return resultArray;
}
} // namespace

extern "C"
{

JNIEXPORT jobjectArray JNICALL
Java_com_mapswithme_maps_editor_OsmOAuth_nativeAuthWithPassword(JNIEnv * env, jclass clazz, jint authType,
                                                                jstring login, jstring password)
{
  OsmOAuth auth = authForType(authType);
  auto authResult = auth.AuthorizePassword(ToNativeString(env, login), ToNativeString(env, password));
  LOG(LINFO, ("Auth result : ", authResult));
  return authResult == OsmOAuth::AuthResult::OK ? ToStringArray(env, auth.GetToken())
                                                : nullptr;
}

JNIEXPORT jobjectArray JNICALL
Java_com_mapswithme_maps_editor_OsmOAuth_nativeAuthWithWebviewToken(JNIEnv * env, jclass clazz, jint authType,
                                                                    jstring secret, jstring token, jstring verifier)
{
  OsmOAuth auth = authForType(authType);
  TKeySecret outKeySecret;
  TKeySecret inKeySecret(ToNativeString(env, secret), ToNativeString(env, token));
  auto authResult = auth.FinishAuthorization(inKeySecret, ToNativeString(env, verifier), outKeySecret);
  LOG(LINFO, ("Auth result : ", authResult));
  if (authResult != OsmOAuth::AuthResult::OK)
    return nullptr;
  auth.FinishAuthorization(inKeySecret, ToNativeString(env, token), outKeySecret);
  return ToStringArray(env, outKeySecret);
}

JNIEXPORT jobjectArray JNICALL
Java_com_mapswithme_maps_editor_OsmOAuth_nativeGetFacebookAuthUrl(JNIEnv * env, jclass clazz, jint authType)
{
  OsmOAuth::TUrlKeySecret keySecret = authForType(authType).GetFacebookOAuthURL();
  return ToStringArray(env, keySecret.first, keySecret.second.first, keySecret.second.second);
}

JNIEXPORT jobjectArray JNICALL
Java_com_mapswithme_maps_editor_OsmOAuth_nativeAuthGoogle(JNIEnv * env, jclass clazz, jint authType)
{
  OsmOAuth::TUrlKeySecret keySecret = authForType(authType).GetGoogleOAuthURL();
  return ToStringArray(env, keySecret.first, keySecret.second.first, keySecret.second.second);
}
} // extern "C"
