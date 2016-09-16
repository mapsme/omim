#include "Platform.hpp"

#include "../core/jni_helper.hpp"

#include "platform/settings.hpp"

#include "base/logging.hpp"
#include "base/stl_add.hpp"

#include "std/algorithm.hpp"


string Platform::UniqueClientId() const
{
  string res;
  if (!settings::Get("UniqueClientID", res))
  {
    JNIEnv * env = jni::GetEnv();
    if (!env)
      return string();

    jclass uuidClass = env->FindClass("java/util/UUID");
    ASSERT(uuidClass, ("Can't find java class java/util/UUID"));

    jmethodID randomUUIDId = jni::GetStaticMethodID(env, uuidClass, "randomUUID", "()Ljava/util/UUID;");
    jobject uuidInstance = env->CallStaticObjectMethod(uuidClass, randomUUIDId);
    ASSERT(uuidInstance, ("UUID.randomUUID() returned NULL"));

    jmethodID toStringId = env->GetMethodID(uuidClass, "toString", "()Ljava/lang/String;");
    ASSERT(toStringId, ("Can't find java/util/UUID.toString() method"));

    jstring uuidString = (jstring)env->CallObjectMethod(uuidInstance, toStringId);
    ASSERT(uuidString, ("UUID.toString() returned NULL"));

    char const * uuidUtf8 = env->GetStringUTFChars(uuidString, 0);

    if (uuidUtf8 != 0)
    {
      res = uuidUtf8;
      env->ReleaseStringUTFChars(uuidString, uuidUtf8);
    }

    res = HashUniqueID(res);

    settings::Set("UniqueClientID", res);
  }

  return res;
}

string Platform::GetMemoryInfo() const
{
  JNIEnv * env = jni::GetEnv();
  if (env == nullptr)
    return string();

  static shared_ptr<jobject> classMemLogging = jni::make_global_ref(env->FindClass("com/mapswithme/util/log/MemLogging"));
  ASSERT(classMemLogging, ());

  static jmethodID const getMemoryInfoId = jni::GetStaticMethodID(env, static_cast<jclass>(*classMemLogging), "getMemoryInfo", "()Ljava/lang/String;");
  jstring const memInfoString = (jstring)env->CallStaticObjectMethod(static_cast<jclass>(*classMemLogging), getMemoryInfoId);
  ASSERT(memInfoString, ());

  return jni::ToNativeString(env, memInfoString);
}

void Platform::RunOnGuiThread(TFunctor const & fn)
{
  android::Platform::Instance().RunOnGuiThread(fn);
}

void Platform::SendPushWooshTag(string const & tag)
{
  SendPushWooshTag(tag, vector<string>{ "1" });
}

void Platform::SendPushWooshTag(string const & tag, string const & value)
{
  SendPushWooshTag(tag, vector<string>{ value });
}

void Platform::SendPushWooshTag(string const & tag, vector<string> const & values)
{
  android::Platform::Instance().SendPushWooshTag(tag, values);
}

void Platform::SendMarketingEvent(string const & tag, map<string, string> const & params)
{
  // TODO: Add implementation.
}

Platform::EConnectionType Platform::ConnectionStatus()
{
  JNIEnv * env = jni::GetEnv();
  if (env == nullptr)
    return EConnectionType::CONNECTION_NONE;

  static shared_ptr<jobject> clazzConnectionState = jni::make_global_ref(env->FindClass("com/mapswithme/util/ConnectionState"));
  ASSERT(clazzConnectionState, ());

  static jmethodID const getConnectionMethodId = jni::GetStaticMethodID(env, static_cast<jclass>(*clazzConnectionState), "getConnectionState", "()B");
  return static_cast<Platform::EConnectionType>(env->CallStaticByteMethod(static_cast<jclass>(*clazzConnectionState), getConnectionMethodId));
}

namespace android
{
  void Platform::Initialize(JNIEnv * env,
                            jobject functorProcessObject,
                            jstring apkPath, jstring storagePath,
                            jstring tmpPath, jstring obbGooglePath,
                            jstring flavorName, jstring buildType,
                            bool isTablet)
  {
    m_functorProcessObject = env->NewGlobalRef(functorProcessObject);
    jclass const functorProcessClass = env->GetObjectClass(functorProcessObject);
    m_functorProcessMethod = env->GetMethodID(functorProcessClass, "forwardToMainThread", "(J)V");
    m_sendPushWooshTagsMethod = env->GetMethodID(functorProcessClass, "sendPushWooshTags", "(Ljava/lang/String;[Ljava/lang/String;)V");

    string const flavor = jni::ToNativeString(env, flavorName);
    string const build = jni::ToNativeString(env, buildType);
    LOG(LINFO, ("Flavor name:", flavor));
    LOG(LINFO, ("Build type name:", build));

    if (build == "beta" || build == "debug")
      m_androidDefResScope = "fwr";
    else if (flavor.find("google") == 0)
      m_androidDefResScope = "ferw";
    else if (flavor.find("amazon") == 0 || flavor.find("samsung") == 0) // optimization to read World mwm-s faster
      m_androidDefResScope = "frw";
    else
      m_androidDefResScope = "fwr";

    m_isTablet = isTablet;
    m_resourcesDir = jni::ToNativeString(env, apkPath);
    m_tmpDir = jni::ToNativeString(env, tmpPath);
    m_writableDir = jni::ToNativeString(env, storagePath);

    string const obbPath = jni::ToNativeString(env, obbGooglePath);
    Platform::FilesList files;
    GetFilesByExt(obbPath, ".obb", files);
    m_extResFiles.clear();
    for (size_t i = 0; i < files.size(); ++i)
      m_extResFiles.push_back(obbPath + files[i]);

    LOG(LINFO, ("Apk path = ", m_resourcesDir));
    LOG(LINFO, ("Writable path = ", m_writableDir));
    LOG(LINFO, ("Temporary path = ", m_tmpDir));
    LOG(LINFO, ("OBB Google path = ", obbPath));
    LOG(LINFO, ("OBB Google files = ", files));

    // IMPORTANT: This method SHOULD be called from UI thread to cache static jni ID-s inside.
    (void) ConnectionStatus();
  }

  void Platform::ProcessFunctor(jlong functionPointer)
  {
    TFunctor * fn = reinterpret_cast<TFunctor *>(functionPointer);
    (*fn)();
    delete fn;
  }

  void Platform::OnExternalStorageStatusChanged(bool isAvailable)
  {
  }

  string Platform::GetStoragePathPrefix() const
  {
    size_t const count = m_writableDir.size();
    ASSERT_GREATER ( count, 2, () );

    size_t const i = m_writableDir.find_last_of('/', count-2);
    ASSERT_GREATER ( i, 0, () );

    return m_writableDir.substr(0, i);
  }

  void Platform::SetWritableDir(string const & dir)
  {
    m_writableDir = dir;
    settings::Set("StoragePath", m_writableDir);
    LOG(LINFO, ("Writable path = ", m_writableDir));
  }

  void Platform::SetSettingsDir(string const & dir)
  {
    m_settingsDir = dir;
    LOG(LINFO, ("Settings path = ", m_settingsDir));
  }

  bool Platform::HasAvailableSpaceForWriting(uint64_t size) const
  {
    return (GetWritableStorageStatus(size) == ::Platform::STORAGE_OK);
  }

  Platform & Platform::Instance()
  {
    static Platform platform;
    return platform;
  }

  void Platform::RunOnGuiThread(TFunctor const & fn)
  {
    // Pointer will be deleted in Platform::ProcessFunctor
    TFunctor * functor = new TFunctor(fn);
    jni::GetEnv()->CallVoidMethod(m_functorProcessObject, m_functorProcessMethod, reinterpret_cast<jlong>(functor));
  }

  void Platform::SendPushWooshTag(string const & tag, vector<string> const & values)
  {
    if (values.empty())
      return;

    JNIEnv * env = jni::GetEnv();
    env->CallVoidMethod(m_functorProcessObject, m_sendPushWooshTagsMethod,
                        jni::TScopedLocalRef(env, jni::ToJavaString(env, tag)).get(),
                        jni::TScopedLocalObjectArrayRef(env, jni::ToJavaStringArray(env, values)).get());
  }
} // namespace android

Platform & GetPlatform()
{
  return android::Platform::Instance();
}
