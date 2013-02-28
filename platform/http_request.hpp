#pragma once

#include "std/cstdint.hpp"
#include "std/function.hpp"
#include "std/string.hpp"
#include "std/vector.hpp"
#include "std/utility.hpp"

namespace downloader
{

/// Request in progress will be canceled on delete
class HttpRequest
{
public:
  enum StatusT
  {
    EInProgress,
    ECompleted,
    EFailed
  };

  /// <current, total>, total can be -1 if size is unknown
  typedef pair<int64_t, int64_t> ProgressT;
  typedef function<void (HttpRequest &)> CallbackT;

protected:
  StatusT m_status;
  ProgressT m_progress;
  CallbackT m_onFinish;
  CallbackT m_onProgress;

  explicit HttpRequest(CallbackT const & onFinish, CallbackT const & onProgress);

public:
  virtual ~HttpRequest() = 0;

  StatusT Status() const { return m_status; }
  ProgressT const & Progress() const { return m_progress; }
  /// Either file path (for chunks) or downloaded data
  virtual string const & Data() const = 0;

  /// Response saved to memory buffer and retrieved with Data()
  static HttpRequest * Get(string const & url,
                           CallbackT const & onFinish,
                           CallbackT const & onProgress = CallbackT());

  /// Content-type for request is always "application/json"
  static HttpRequest * PostJson(string const & url, string const & postData,
                                CallbackT const & onFinish,
                                CallbackT const & onProgress = CallbackT());

  /// @name Download file from urls to filePath.
  /// @param[in]  fileSize  Correct file size (needed for resuming and reserving).
  //@{
  /// Using cross-platform downloader.
  static HttpRequest * GetFile(vector<string> const & urls,
                               string const & filePath, int64_t fileSize,
                               CallbackT const & onFinish,
                               CallbackT const & onProgress = CallbackT(),
                               int64_t chunkSize = 512 * 1024,
                               bool doCleanOnCancel = true);

  /// Using native downloader for Android (default is cross-platform).
  /// @param[in]  fileName  Using as title for downloading UI.
  static HttpRequest * Download(vector<string> const & urls, string const & filePath,
                                string const & fileName, int64_t fileSize,
                                CallbackT const & onFinish, CallbackT const & onProgress,
                                int64_t chunkSize, bool doCleanOnCancel);
  //@}
};

} // namespace downloader
