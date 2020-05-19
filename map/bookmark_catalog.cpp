#include "map/bookmark_catalog.hpp"

#include "map/bookmark_helpers.hpp"

#include "web_api/request_headers.hpp"
#include "web_api/utils.hpp"

#include "platform/http_payload.hpp"
#include "platform/http_uploader.hpp"
#include "platform/platform.hpp"
#include "platform/preferred_languages.hpp"

#include "coding/serdes_json.hpp"
#include "coding/sha1.hpp"
#include "coding/zip_creator.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"
#include "base/logging.hpp"
#include "base/string_utils.hpp"
#include "base/visitor.hpp"

#include <cstring>
#include <sstream>
#include <utility>

#include "private.h"

namespace
{
std::string const kCatalogFrontendServer = BOOKMARKS_CATALOG_FRONT_URL;
std::string const kCatalogDownloadServer = BOOKMARKS_CATALOG_DOWNLOAD_URL;
std::string const kCatalogEditorServer = BOOKMARKS_CATALOG_EDITOR_URL;

std::string BuildCatalogDownloadUrl(std::string const & serverId)
{
  if (kCatalogDownloadServer.empty())
    return {};
  return kCatalogDownloadServer + serverId;
}

std::string BuildTagsUrl(std::string const & language)
{
  if (kCatalogEditorServer.empty())
    return {};
  return kCatalogEditorServer + "editor/v2/tags/?lang=" + language;
}

std::string BuildCustomPropertiesUrl(std::string const & language)
{
  if (kCatalogEditorServer.empty())
    return {};
  return kCatalogEditorServer + "editor/properties/?lang=" + language;
}

std::string BuildHashUrl()
{
  if (kCatalogFrontendServer.empty())
    return {};
  return kCatalogFrontendServer + "storage/create_hash";
}

std::string BuildUploadUrl()
{
  if (kCatalogFrontendServer.empty())
    return {};
  return kCatalogFrontendServer + "storage/upload";
}

std::string BuildWebEditorUrl(std::string const & serverId, std::string const & language)
{
  if (kCatalogEditorServer.empty())
    return {};
  return kCatalogEditorServer + "webeditor/" + language + "/edit/" + serverId;
}

std::string BuildPingUrl()
{
  if (kCatalogFrontendServer.empty())
    return {};
  return kCatalogFrontendServer + "storage/ping";
}

std::string BuildDeleteRequestUrl()
{
  if (kCatalogFrontendServer.empty())
    return {};
  return kCatalogFrontendServer + "storage/kmls_to_delete";
}

struct SubtagData
{
  std::string m_name;
  std::string m_color;
  std::string m_translation;
  DECLARE_VISITOR(visitor(m_name, "name"),
                  visitor(m_color, "color"),
                  visitor(m_translation, "translation"))
};

struct TagData
{
  std::string m_name;
  std::vector<SubtagData> m_subtags;
  DECLARE_VISITOR(visitor(m_name, "translation"),
                  visitor(m_subtags, "subtags"))
};

struct TagsMeta
{
  uint32_t m_maxTags;

  DECLARE_VISITOR(visitor(m_maxTags, "max_for_bundle"))
};

struct TagsResponseData
{
  std::vector<TagData> m_tags;
  TagsMeta m_meta;

  DECLARE_VISITOR(visitor(m_tags, "data"),
                  visitor(m_meta, "meta"))
};

BookmarkCatalog::Tag::Color ExtractColor(std::string const & c)
{
  BookmarkCatalog::Tag::Color result;

  auto const components = c.size() / 2;
  if (components < result.size())
    return {};

  for (size_t i = 0; i < result.size(); i++)
    result[i] = static_cast<float>(std::stoi(c.substr(i * 2, 2), nullptr, 16)) / 255;
  return result;
}

struct CustomPropertyOptionData
{
  std::string m_value;
  std::string m_name;
  DECLARE_VISITOR(visitor(m_value, "value"),
                  visitor(m_name, "name"))
};

struct CustomPropertyData
{
  std::string m_key;
  std::string m_name;
  bool m_isRequired;
  std::vector<CustomPropertyOptionData> m_options;
  DECLARE_VISITOR(visitor(m_key, "key"),
                  visitor(m_name, "name"),
                  visitor(m_isRequired, "required"),
                  visitor(m_options, "options"))
};

struct CustomPropertiesData
{
  std::vector<CustomPropertyData> m_properties;

  DECLARE_VISITOR(visitor(m_properties))
};

struct HashResponseData
{
  std::string m_hash;
  DECLARE_VISITOR(visitor(m_hash, "hash"))
};

struct DeleteRequestData
{
  DeleteRequestData(std::string const & deviceId, std::string const & userId,
                    std::vector<std::string> const & serverIds)
    : m_deviceId(deviceId)
    , m_userId(userId)
    , m_serverIds(serverIds)
  {}

  std::string m_deviceId;
  std::string m_userId;
  std::vector<std::string> m_serverIds;

  DECLARE_VISITOR(visitor(m_deviceId, "device_id"),
                  visitor(m_userId, "user_id"),
                  visitor(m_serverIds, "server_ids"))
};

struct DeleteRequestResponseData
{
  std::vector<std::string> m_serverIds;

  DECLARE_VISITOR(visitor(m_serverIds))
};

int constexpr kInvalidHash = -1;

int RequestNewServerId(std::string const & accessToken,
                       std::string & serverId, std::string & errorString)
{
  auto const hashUrl = BuildHashUrl();
  if (hashUrl.empty())
    return kInvalidHash;
  platform::HttpClient request(hashUrl);
  request.SetRawHeaders(web_api::GetDefaultCatalogHeaders());
  request.SetRawHeader("Accept", "application/json");
  request.SetRawHeader("Authorization", "Bearer " + accessToken);
  request.SetHttpMethod("POST");
  if (request.RunHttpRequest())
  {
    auto const resultCode = request.ErrorCode();
    if (resultCode >= 200 && resultCode < 300)  // Ok.
    {
      HashResponseData hashResponseData;
      try
      {
        coding::DeserializerJson des(request.ServerResponse());
        des(hashResponseData);
      }
      catch (coding::DeserializerJson::Exception const & ex)
      {
        LOG(LWARNING, ("Hash request deserialization error:", ex.Msg()));
        return kInvalidHash;
      }

      serverId = hashResponseData.m_hash;
      return resultCode;
    }
    errorString = request.ServerResponse();
  }
  return kInvalidHash;
}
}  // namespace

void BookmarkCatalog::RegisterByServerId(std::string const & id)
{
  if (id.empty())
    return;

  m_registeredInCatalog.insert(id);
}

void BookmarkCatalog::UnregisterByServerId(std::string const & id)
{
  if (id.empty())
    return;

  m_registeredInCatalog.erase(id);
}

bool BookmarkCatalog::IsDownloading(std::string const & id) const
{
  return m_downloadingIds.find(id) != m_downloadingIds.cend();
}

bool BookmarkCatalog::HasDownloaded(std::string const & id) const
{
  return m_registeredInCatalog.find(id) != m_registeredInCatalog.cend();
}

std::set<std::string> const & BookmarkCatalog::GetDownloadedIds() const
{
  return m_registeredInCatalog;
}

void BookmarkCatalog::Download(std::string const & id, std::string const & accessToken,
                               DownloadStartCallback && startHandler,
                               DownloadFinishCallback && finishHandler)
{
  if (IsDownloading(id) || HasDownloaded(id))
    return;

  m_downloadingIds.insert(id);

  static uint32_t counter = 0;
  auto const path = base::JoinPath(GetPlatform().TmpDir(), "file" + strings::to_string(++counter));

  platform::RemoteFile remoteFile(BuildCatalogDownloadUrl(id), accessToken, GetHeaders());
  remoteFile.DownloadAsync(path, [startHandler = std::move(startHandler)](std::string const &)
  {
    if (startHandler)
      startHandler();
  }, [this, id, finishHandler = std::move(finishHandler)] (platform::RemoteFile::Result && result,
                                                           std::string const & filePath) mutable
  {
    GetPlatform().RunTask(Platform::Thread::Gui, [this, id, result = std::move(result), filePath,
                                                  finishHandler = std::move(finishHandler)]() mutable
    {
      m_downloadingIds.erase(id);

      DownloadResult downloadResult;
      switch (result.m_status)
      {
      case platform::RemoteFile::Status::Ok:
        downloadResult = DownloadResult::Success;
        break;
      case platform::RemoteFile::Status::Forbidden:
        if (m_onInvalidToken)
          m_onInvalidToken();
        downloadResult = DownloadResult::AuthError;
        break;
      case platform::RemoteFile::Status::NotFound:
        downloadResult = DownloadResult::ServerError;
        break;
      case platform::RemoteFile::Status::NetworkError:
        if (result.m_httpCode == 402)
          downloadResult = DownloadResult::NeedPayment;
        else
          downloadResult = DownloadResult::NetworkError;
        break;
      case platform::RemoteFile::Status::DiskError:
        downloadResult = DownloadResult::DiskError;
        break;
      }

      if (finishHandler)
        finishHandler(downloadResult, result.m_description, filePath);
    });
  });
}

std::string BookmarkCatalog::GetDownloadUrl(std::string const & serverId) const
{
  return BuildCatalogDownloadUrl(serverId);
}

std::string BookmarkCatalog::GetWebEditorUrl(std::string const & serverId,
                                             std::string const & language) const
{
  return BuildWebEditorUrl(serverId, language);
}

std::string BookmarkCatalog::GetFrontendUrl(UTM utm) const
{
  return InjectUTM(kCatalogFrontendServer + languages::GetCurrentNorm() + "/v3/mobilefront/", utm);
}

void BookmarkCatalog::RequestTagGroups(std::string const & language,
                                       BookmarkCatalog::TagGroupsCallback && callback) const
{
  auto const tagsUrl = BuildTagsUrl(language);
  if (tagsUrl.empty())
  {
    if (callback)
      callback(false /* success */, {}, 0 /* maxTagsCount */);
    return;
  }

  GetPlatform().RunTask(Platform::Thread::Network, [tagsUrl, callback = std::move(callback)]()
  {
    platform::HttpClient request(tagsUrl);
    request.SetRawHeaders(web_api::GetDefaultCatalogHeaders());
    request.SetRawHeader("Accept", "application/json");
    if (request.RunHttpRequest())
    {
      auto const resultCode = request.ErrorCode();
      uint32_t maxTagsCount = 0;
      if (resultCode >= 200 && resultCode < 300)  // Ok.
      {
        TagsResponseData tagsResponseData;
        try
        {
          coding::DeserializerJson des(request.ServerResponse());
          des(tagsResponseData);
        }
        catch (coding::DeserializerJson::Exception const & ex)
        {
          LOG(LWARNING, ("Tags request deserialization error:", ex.Msg()));
          if (callback)
            callback(false /* success */, {}, 0 /* maxTagsCount */);
          return;
        }

        TagGroups result;
        result.reserve(tagsResponseData.m_tags.size());
        for (auto const & t : tagsResponseData.m_tags)
        {
          TagGroup group;
          group.m_name = t.m_name;
          group.m_tags.reserve(t.m_subtags.size());
          for (auto const & st : t.m_subtags)
          {
            Tag tag;
            tag.m_id = st.m_name;
            tag.m_name = st.m_translation;
            tag.m_color = ExtractColor(st.m_color);
            group.m_tags.push_back(std::move(tag));
          }
          result.push_back(std::move(group));

          maxTagsCount = tagsResponseData.m_meta.m_maxTags;
        }
        if (callback)
          callback(true /* success */, result, maxTagsCount);
        return;
      }
      else
      {
        LOG(LWARNING, ("Tags request error. Code =", resultCode));
      }
    }
    if (callback)
      callback(false /* success */, {}, 0 /* maxTagsCount */);
  });
}

void BookmarkCatalog::RequestCustomProperties(std::string const & language,
                                              CustomPropertiesCallback && callback) const
{
  auto const url = BuildCustomPropertiesUrl(language);
  if (url.empty())
  {
    if (callback)
      callback(false /* success */, {});
    return;
  }

  GetPlatform().RunTask(Platform::Thread::Network, [url, callback = std::move(callback)]()
  {
    platform::HttpClient request(url);
    request.SetRawHeaders(web_api::GetDefaultCatalogHeaders());
    request.SetRawHeader("Accept", "application/json");
    if (request.RunHttpRequest())
    {
      auto const resultCode = request.ErrorCode();
      if (resultCode >= 200 && resultCode < 300)  // Ok.
      {
        CustomPropertiesData responseData;
        try
        {
          coding::DeserializerJson des(request.ServerResponse());
          des(responseData);
        }
        catch (coding::DeserializerJson::Exception const & ex)
        {
          LOG(LWARNING, ("Custom properties request deserialization error:", ex.Msg()));
          if (callback)
            callback(false /* success */, {});
          return;
        }

        CustomProperties result;
        result.reserve(responseData.m_properties.size());
        for (auto const & p : responseData.m_properties)
        {
          CustomProperty prop;
          prop.m_key = p.m_key;
          prop.m_name = p.m_name;
          prop.m_isRequired = p.m_isRequired;
          prop.m_options.reserve(p.m_options.size());
          for (auto const & o : p.m_options)
          {
            CustomProperty::Option option;
            option.m_value = o.m_value;
            option.m_name = o.m_name;
            prop.m_options.push_back(std::move(option));
          }
          result.push_back(std::move(prop));
        }
        if (callback)
          callback(true /* success */, result);
        return;
      }
      else
      {
        LOG(LWARNING, ("Custom properties request error. Code =", resultCode));
      }
    }
    if (callback)
      callback(false /* success */, {});
  });
}

void BookmarkCatalog::Upload(UploadData uploadData, std::string const & accessToken,
                             std::shared_ptr<kml::FileData> fileData, std::string const & pathToKmb,
                             UploadSuccessCallback && uploadSuccessCallback,
                             UploadErrorCallback && uploadErrorCallback)
{
  CHECK(fileData != nullptr, ());
  CHECK_EQUAL(uploadData.m_serverId, fileData->m_serverId, ());

  if (accessToken.empty())
  {
    if (uploadErrorCallback)
      uploadErrorCallback(UploadResult::AuthError, {});
    return;
  }

  if (fileData->m_categoryData.m_accessRules == kml::AccessRules::Paid)
  {
    if (uploadErrorCallback)
      uploadErrorCallback(UploadResult::InvalidCall, "Could not upload paid bookmarks.");
    return;
  }

  if (!fileData->m_categoryData.m_authorId.empty() &&
      fileData->m_categoryData.m_authorId != uploadData.m_userId)
  {
    if (uploadErrorCallback)
    {
      uploadErrorCallback(UploadResult::AccessError, "Could not upload not own bookmarks.");
    }
    return;
  }

  GetPlatform().RunTask(Platform::Thread::File, [this, uploadData = std::move(uploadData), accessToken, fileData,
                                                 pathToKmb, uploadSuccessCallback = std::move(uploadSuccessCallback),
                                                 uploadErrorCallback = std::move(uploadErrorCallback)]() mutable
  {
    if (!GetPlatform().IsFileExistsByFullPath(pathToKmb))
    {
      if (uploadErrorCallback)
        uploadErrorCallback(UploadResult::InvalidCall, "Could not find the file to upload.");
      return;
    }

    auto const originalSha1 = coding::SHA1::CalculateBase64(pathToKmb);
    if (originalSha1.empty())
    {
      if (uploadErrorCallback)
        uploadErrorCallback(UploadResult::InvalidCall, "Could not calculate hash for the uploading file.");
      return;
    }

    GetPlatform().RunTask(Platform::Thread::Network, [this, uploadData = std::move(uploadData), accessToken,
                                                      pathToKmb, fileData, originalSha1,
                                                      uploadSuccessCallback = std::move(uploadSuccessCallback),
                                                      uploadErrorCallback = std::move(uploadErrorCallback)]() mutable
    {
      if (uploadData.m_serverId.empty())
      {
        // Request new server id.
        std::string serverId;
        std::string errorString;
        auto const resultCode = RequestNewServerId(accessToken, serverId, errorString);
        if (resultCode == 403)
        {
          if (m_onInvalidToken)
            m_onInvalidToken();

          if (uploadErrorCallback)
            uploadErrorCallback(UploadResult::AuthError, errorString);
          return;
        }
        if (resultCode < 200 || resultCode >= 300)
        {
          if (uploadErrorCallback)
            uploadErrorCallback(UploadResult::NetworkError, errorString);
          return;
        }
        uploadData.m_serverId = serverId;
      }

      // Embed necessary data to KML.
      fileData->m_serverId = uploadData.m_serverId;
      fileData->m_categoryData.m_accessRules = uploadData.m_accessRules;
      fileData->m_categoryData.m_authorName = uploadData.m_userName;
      fileData->m_categoryData.m_authorId = uploadData.m_userId;

      // Save KML.
      auto const filePath = base::JoinPath(GetPlatform().TmpDir(), uploadData.m_serverId + kKmlExtension);
      if (!SaveKmlFileSafe(*fileData, filePath, KmlFileType::Text))
      {
        FileWriter::DeleteFileX(filePath);
        if (uploadErrorCallback)
          uploadErrorCallback(UploadResult::InvalidCall, "Could not save the uploading file.");
        return;
      }

      // Zip KML.
      auto const zippedFilePath = base::JoinPath(GetPlatform().TmpDir(), uploadData.m_serverId + kKmzExtension);
      if (!CreateZipFromPathDeflatedAndDefaultCompression(filePath, zippedFilePath))
      {
        FileWriter::DeleteFileX(filePath);
        if (uploadErrorCallback)
          uploadErrorCallback(UploadResult::InvalidCall, "Could not zip the uploading file.");
        return;
      }
      FileWriter::DeleteFileX(filePath);

      // Upload zipped KML.
      platform::HttpPayload payload;
      payload.m_url = BuildUploadUrl();
      payload.m_headers = {{"Authorization", "Bearer " + accessToken},
                           {"User-Agent", GetPlatform().GetAppUserAgent()}};

      payload.m_params = {{"author_id", uploadData.m_userId},
                          {"bundle_hash", uploadData.m_serverId},
                          {"locale", languages::GetCurrentOrig()}};
      payload.m_fileKey = "file_contents";
      payload.m_filePath = zippedFilePath;

      platform::HttpUploader request(payload);
      auto const uploadCode = request.Upload();
      if (uploadCode.m_httpCode >= 200 && uploadCode.m_httpCode < 300)
      {
        // Update KML.
        GetPlatform().RunTask(Platform::Thread::File,
                              [pathToKmb, fileData, originalSha1,
                               uploadSuccessCallback = std::move(uploadSuccessCallback)]() mutable
        {
          bool const originalFileExists = GetPlatform().IsFileExistsByFullPath(pathToKmb);
          bool originalFileUnmodified = false;
          if (originalFileExists)
            originalFileUnmodified = (originalSha1 == coding::SHA1::CalculateBase64(pathToKmb));
          if (uploadSuccessCallback)
          {
            uploadSuccessCallback(UploadResult::Success, fileData,
                                  originalFileExists, originalFileUnmodified);
          }
        });
      }
      else if (uploadCode.m_httpCode == 400)
      {
        if (uploadErrorCallback)
          uploadErrorCallback(UploadResult::MalformedDataError, uploadCode.m_description);
      }
      else if (uploadCode.m_httpCode == 403)
      {
        if (m_onInvalidToken)
          m_onInvalidToken();

        if (uploadErrorCallback)
          uploadErrorCallback(UploadResult::AuthError, uploadCode.m_description);
      }
      else if (uploadCode.m_httpCode == 409)
      {
        if (uploadErrorCallback)
          uploadErrorCallback(UploadResult::AccessError, uploadCode.m_description);
      }
      else if (uploadCode.m_httpCode >= 500 && uploadCode.m_httpCode < 600)
      {
        if (uploadErrorCallback)
          uploadErrorCallback(UploadResult::ServerError, uploadCode.m_description);
      }
      else
      {
        if (uploadErrorCallback)
          uploadErrorCallback(UploadResult::NetworkError, uploadCode.m_description);
      }
      FileWriter::DeleteFileX(zippedFilePath);
    });
  });
}

void BookmarkCatalog::Ping(PingCallback && callback) const
{
  auto const url = BuildPingUrl();
  if (url.empty())
  {
    if (callback)
      callback(false /* isSuccessful */);
    return;
  }

  GetPlatform().RunTask(Platform::Thread::Network, [url, callback = std::move(callback)]()
  {
    platform::HttpClient request(url);
    request.SetRawHeaders(web_api::GetDefaultCatalogHeaders());
    uint32_t constexpr kPingTimeoutInSec = 10;
    request.SetTimeout(kPingTimeoutInSec);
    if (request.RunHttpRequest())
    {
      auto constexpr kExpectedResponse = "pong";
      auto const resultCode = request.ErrorCode();
      if (callback && resultCode >= 200 && resultCode < 300 &&
          request.ServerResponse() == kExpectedResponse)
      {
        callback(true /* isSuccessful */);
        return;
      }
    }
    if (callback)
      callback(false /* isSuccessful */);
  });
}

void BookmarkCatalog::RequestBookmarksToDelete(std::string const & accessToken, std::string const & userId,
                                               std::vector<std::string> const & serverIds,
                                               BookmarksToDeleteCallback && callback) const
{
  auto const url = BuildDeleteRequestUrl();
  if (url.empty())
  {
    if (callback)
      callback({});
    return;
  }

  GetPlatform().RunTask(Platform::Thread::Network,
                        [url, accessToken, userId, serverIds, callback = std::move(callback)]()
  {
    platform::HttpClient request(url);
    request.SetRawHeaders(web_api::GetDefaultCatalogHeaders());
    request.SetRawHeader("Accept", "application/json");
    if (!accessToken.empty())
      request.SetRawHeader("Authorization", "Bearer " + accessToken);
    request.SetHttpMethod("POST");

    std::string jsonStr;
    {
      using Sink = MemWriter<std::string>;
      Sink sink(jsonStr);
      coding::SerializerJson<Sink> serializer(sink);
      serializer(DeleteRequestData(web_api::DeviceId(), userId, serverIds));
    }
    request.SetBodyData(std::move(jsonStr), "application/json");

    uint32_t constexpr kTimeoutInSec = 5;
    request.SetTimeout(kTimeoutInSec);
    if (request.RunHttpRequest())
    {
      auto const resultCode = request.ErrorCode();
      if (resultCode >= 200 && resultCode < 300)
      {
        DeleteRequestResponseData responseData;
        try
        {
          coding::DeserializerJson des(request.ServerResponse());
          des(responseData);
        }
        catch (coding::DeserializerJson::Exception const & ex)
        {
          LOG(LERROR, ("Bookmarks-to-delete request deserialization error:", ex.Msg()));
          callback({});
          return;
        }

        callback(responseData.m_serverIds);
        return;
      }
    }
    callback({});
  });
}

void BookmarkCatalog::SetInvalidTokenHandler(InvalidTokenHandler && onInvalidToken)
{
  m_onInvalidToken = std::move(onInvalidToken);
}

void BookmarkCatalog::SetHeadersProvider(HeadersProvider const & provider)
{
  m_headersProvider = provider;
}

platform::HttpClient::Headers BookmarkCatalog::GetHeaders() const
{
  return m_headersProvider();
}
