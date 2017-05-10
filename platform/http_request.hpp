#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <utility>

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
  typedef std::pair<int64_t, int64_t> ProgressT;
  typedef std::function<void (HttpRequest &)> CallbackT;

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
  virtual std::string const & Data() const = 0;

  /// Response saved to memory buffer and retrieved with Data()
  static HttpRequest * Get(std::string const & url,
                           CallbackT const & onFinish,
                           CallbackT const & onProgress = CallbackT());

  /// Content-type for request is always "application/json"
  static HttpRequest * PostJson(std::string const & url, std::string const & postData,
                                CallbackT const & onFinish,
                                CallbackT const & onProgress = CallbackT());

  /// Download file to filePath.
  /// @param[in]  fileSize  Correct file size (needed for resuming and reserving).
  static HttpRequest * GetFile(std::vector<std::string> const & urls,
                               std::string const & filePath, int64_t fileSize,
                               CallbackT const & onFinish,
                               CallbackT const & onProgress = CallbackT(),
                               int64_t chunkSize = 512 * 1024,
                               bool doCleanOnCancel = true);
};

} // namespace downloader
