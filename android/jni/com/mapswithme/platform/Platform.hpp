#pragma once

#include <jni.h>

#include "platform/platform.hpp"

namespace android
{
  class Platform : public ::Platform
  {
  public:
    Platform() {}
    void Initialize(JNIEnv * env,
                    jobject functorProcessObject,
                    jstring apkPath, jstring storagePath,
                    jstring tmpPath, jstring obbGooglePath,
                    jstring flavorName, jstring buildType,
                    bool isYota, bool isTablet);

    void ProcessFunctor(jlong functionPointer);

    void OnExternalStorageStatusChanged(bool isAvailable);

    /// get storage path without ending "/MapsWithMe/"
    string GetStoragePathPrefix() const;
    /// assign storage path (should contain ending "/MapsWithMe/")
    void SetStoragePath(string const & path);

    bool HasAvailableSpaceForWriting(uint64_t size) const;
    void RunOnGuiThread(TFunctor const & fn);

    static Platform & Instance();

  private:
    jobject m_functorProcessObject;
    jmethodID m_functorProcessMethod;
  };
}
