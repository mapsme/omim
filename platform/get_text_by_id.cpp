#include "platform/get_text_by_id.hpp"
#include "platform/platform.hpp"

#include "coding/file_name_utils.hpp"

#include "base/logging.hpp"

#include "3party/jansson/myjansson.hpp"

#include <algorithm>
#include "std/target_os.hpp"

namespace
{
static constexpr char const * kDefaultLanguage = "en";

std::string GetTextSourceString(platform::TextSource textSource)
{
  switch (textSource)
  {
  case platform::TextSource::TtsSound:
    return std::string("sound-strings");
  case platform::TextSource::Countries:
    return std::string("countries-strings");
  case platform::TextSource::Cuisines:
    return std::string("cuisine-strings");
  }
  ASSERT(false, ());
  return std::string();
}
}  // namespace

namespace platform
{
bool GetJsonBuffer(platform::TextSource textSource, std::string const & localeName, std::string & jsonBuffer)
{
  std::string const pathToJson = my::JoinFoldersToPath(
      {GetTextSourceString(textSource), localeName + ".json"}, "localize.json");

  try
  {
    jsonBuffer.clear();
    GetPlatform().GetReader(pathToJson)->ReadAsString(jsonBuffer);
  }
  catch (RootException const & ex)
  {
    LOG(LWARNING, ("Can't open", localeName, "localization file. pathToJson is", pathToJson,
                   ex.what()));
    return false;  // No json file for localeName
  }
  return true;
}

TGetTextByIdPtr MakeGetTextById(std::string const & jsonBuffer, std::string const & localeName)
{
  TGetTextByIdPtr result(new GetTextById(jsonBuffer, localeName));
  if (!result->IsValid())
  {
    ASSERT(false, ("Can't create a GetTextById instance from a json file. localeName=", localeName));
    return nullptr;
  }
  return result;
}

TGetTextByIdPtr GetTextByIdFactory(TextSource textSource, std::string const & localeName)
{
  std::string jsonBuffer;
  if (GetJsonBuffer(textSource, localeName, jsonBuffer))
    return MakeGetTextById(jsonBuffer, localeName);

  if (GetJsonBuffer(textSource, kDefaultLanguage, jsonBuffer))
    return MakeGetTextById(jsonBuffer, kDefaultLanguage);

  ASSERT(false, ("Can't std::find translate for default language. (Lang:", localeName, ")"));
  return nullptr;
}

TGetTextByIdPtr ForTestingGetTextByIdFactory(std::string const & jsonBuffer, std::string const & localeName)
{
  return MakeGetTextById(jsonBuffer, localeName);
}

GetTextById::GetTextById(std::string const & jsonBuffer, std::string const & localeName)
  : m_locale(localeName)
{
  InitFromJson(jsonBuffer);
}

void GetTextById::InitFromJson(std::string const & jsonBuffer)
{
  if (jsonBuffer.empty())
  {
    ASSERT(false, ("No json files found."));
    return;
  }

  my::Json root(jsonBuffer.c_str());
  if (root.get() == nullptr)
  {
    ASSERT(false, ("Cannot parse the json file."));
    return;
  }

  char const * key = nullptr;
  json_t * value = nullptr;
  json_object_foreach(root.get(), key, value)
  {
    ASSERT(key, ());
    ASSERT(value, ());
    char const * const valueStr = json_string_value(value);
    ASSERT(valueStr, ());
    m_localeTexts[key] = valueStr;
  }
  ASSERT_EQUAL(m_localeTexts.size(), json_object_size(root.get()), ());
}

std::string GetTextById::operator()(std::string const & textId) const
{
  auto const textIt = m_localeTexts.find(textId);
  if (textIt == m_localeTexts.end())
    return std::string();
  return textIt->second;
}

TTranslations GetTextById::GetAllSortedTranslations() const
{
  TTranslations all;
  all.reserve(m_localeTexts.size());
  for (auto const & tr : m_localeTexts)
    all.emplace_back(tr.first, tr.second);
  using TValue = TTranslations::value_type;
  std::sort(all.begin(), all.end(), [](TValue const & v1, TValue const & v2) { return v1.second < v2.second; });
  return all;
}
}  // namespace platform
