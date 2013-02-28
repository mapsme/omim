#include "../../../../../platform/http_request.hpp"

#include "../../../../../coding/internal/file_data.hpp"

#include "../../../../../base/assert.hpp"
#include "../../../../../base/logging.hpp"

#include "../core/jni_helper.hpp"


#define DOWNLOAD_FILE_POSTFIX ".downloader"

namespace downloader
{
  class AndroidFileRequest : public HttpRequest
  {
    vector<string> m_urls;
    string m_fileName, m_filePath;
    size_t m_url;

    bool StartDownload()
    {
      if (m_url >= m_urls.size())
        return false;

      JNIEnv * env = jni::GetEnv();
      ASSERT ( env, () );

      jclass klass = env->FindClass("com/mapswithme/maps/MWMApplication");
      ASSERT ( klass, () );

      jmethodID method = env->GetStaticMethodID(klass, "startDownload", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;J)V");
      ASSERT ( method, () );

      string const path = m_filePath + DOWNLOAD_FILE_POSTFIX;

      // Delete current file or DownloadManager will start to download with other name otherwise.
      (void) my::DeleteFileX(path);

      env->CallStaticVoidMethod(klass, method,
          jni::ToJavaString(env, m_fileName),
          jni::ToJavaString(env, m_urls[m_url]),
          jni::ToJavaString(env, path),
          reinterpret_cast<jlong>(this));

      return true;
    }

    void CancelDownload()
    {
      JNIEnv * env = jni::GetEnv();
      ASSERT ( env, () );

      jclass klass = env->FindClass("com/mapswithme/maps/MWMApplication");
      ASSERT ( klass, () );

      jmethodID method = env->GetStaticMethodID(klass, "cancelDownload", "()V");
      ASSERT ( method, () );

      env->CallStaticVoidMethod(klass, method);
    }

  public:
    AndroidFileRequest(vector<string> const & urls, string const & filePath,
                       string const & fileName, int64_t fileSize,
                       CallbackT const & onFinish, CallbackT const & onProgress)
    : HttpRequest(onFinish, onProgress), m_urls(urls), m_fileName(fileName), m_filePath(filePath), m_url(0)
    {
      m_progress.first = 0;
      m_progress.second = fileSize;

      CHECK(StartDownload(), ());
    }

    virtual ~AndroidFileRequest()
    {
      CancelDownload();
    }

    virtual string const & Data() const { return m_filePath; }

    bool OnProgress(size_t size)
    {
      m_progress.first = size;
      m_onProgress(*this);
      return true;
    }

    void OnFinish(long httpCode)
    {
      if (httpCode == 200)
      {
        // Set original file name on success (was downloading in temporary file in case of fail).
        (void) my::DeleteFileX(m_filePath);
        CHECK(my::RenameFileX(m_filePath + DOWNLOAD_FILE_POSTFIX, m_filePath), ());

        m_status = ECompleted;
      }
      else
      {
        LOG(LWARNING, ("AndroidFileRequest HTTP error", httpCode, "for", m_urls[m_url]));

        ++m_url;
        if (StartDownload())
          return;

        m_status = EFailed;
      }

      m_onFinish(*this);
    }

    static bool HasDownloadManager()
    {
      JNIEnv * env = jni::GetEnv();
      ASSERT ( env, () );

      jclass klass = env->FindClass("com/mapswithme/maps/MWMApplication");
      ASSERT ( klass, () );

      jmethodID method = env->GetStaticMethodID(klass, "hasDownloader", "()Z");
      ASSERT ( method, () );

      return env->CallStaticBooleanMethod(klass, method);
    }
  };

  HttpRequest * HttpRequest::Download(vector<string> const & urls, string const & filePath,
                                      string const & fileName, int64_t fileSize,
                                      CallbackT const & onFinish, CallbackT const & onProgress,
                                      int64_t chunkSize, bool doCleanOnCancel)
  {
    if (AndroidFileRequest::HasDownloadManager())
      return new AndroidFileRequest(urls, filePath, fileName, fileSize, onFinish, onProgress);
    else
      return GetFile(urls, filePath, fileSize, onFinish, onProgress, chunkSize, doCleanOnCancel);
  }
}

extern "C"
{
  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_downloader_Downloader_onProgress(JNIEnv * env, jobject thiz,
      jlong size, jlong callback)
  {
    downloader::AndroidFileRequest * cb = reinterpret_cast<downloader::AndroidFileRequest*>(callback);
    cb->OnProgress(size);
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_downloader_Downloader_onFinish(JNIEnv * env, jobject thiz,
      jint httpCode, jlong callback)
  {
    downloader::AndroidFileRequest * cb = reinterpret_cast<downloader::AndroidFileRequest*>(callback);
    cb->OnFinish(httpCode);
  }
}
