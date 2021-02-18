#include "platform/downloader_utils.hpp"

#include "platform/country_defines.hpp"
#include "platform/country_file.hpp"
#include "platform/local_country_file_utils.hpp"

#include "coding/url.hpp"

#include "base/string_utils.hpp"

#include "std/target_os.hpp"

namespace
{
std::string const kDiffsPath = "diffs";
}  // namespace

namespace downloader
{
std::string GetFileDownloadUrl(std::string const & fileName, int64_t dataVersion,
                               uint64_t diffVersion)
{
  if (diffVersion == 0)
    return url::Join(OMIM_OS_NAME, strings::to_string(dataVersion), url::UrlEncode(fileName));

  return url::Join(kDiffsPath, strings::to_string(dataVersion), strings::to_string(diffVersion),
                   url::UrlEncode(fileName));
}

bool IsUrlSupported(std::string const & url)
{
  auto const urlComponents = strings::Tokenize(url, "/");
  if (urlComponents.empty())
    return false;

  if (urlComponents[0] != OMIM_OS_NAME && urlComponents[0] != kDiffsPath)
    return false;

  if (urlComponents[0] == OMIM_OS_NAME && urlComponents.size() != 3)
    return false;

  if (urlComponents[0] == kDiffsPath && urlComponents.size() != 4)
    return false;

  int64_t dataVersion = 0;
  if (!strings::to_int64(urlComponents[1], dataVersion))
    return false;

  if (urlComponents[0] == kDiffsPath)
  {
    int64_t diffVersion = 0;
    if (!strings::to_int64(urlComponents[2], diffVersion))
      return false;
  }

  auto const countryComponents = strings::Tokenize(url::UrlDecode(urlComponents.back()), ".");
  return countryComponents.size() == 2;
}

std::string GetFilePathByUrl(std::string const & url)
{
  auto const urlComponents = strings::Tokenize(url, "/");
  CHECK_GREATER(urlComponents.size(), 2, (urlComponents));
  CHECK_LESS(urlComponents.size(), 5, (urlComponents));

  int64_t dataVersion = 0;
  CHECK(strings::to_int64(urlComponents[1], dataVersion), ());

  auto const countryComponents = strings::Tokenize(url::UrlDecode(urlComponents.back()), ".");
  CHECK_EQUAL(countryComponents.size(), 2, ());

  auto const fileType = urlComponents[0] == kDiffsPath ? MapFileType::Diff : MapFileType::Map;

  return platform::GetFileDownloadPath(dataVersion, platform::CountryFile(countryComponents[0]),
                                       fileType);
}
}  // namespace downloader
