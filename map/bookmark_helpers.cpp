#include "map/bookmark_helpers.hpp"

#include "kml/serdes.hpp"
#include "kml/serdes_binary.hpp"

#include "indexer/categories_holder.hpp"
#include "indexer/classificator.hpp"
#include "indexer/feature_utils.hpp"

#include "platform/platform.hpp"
#include "platform/preferred_languages.hpp"

#include "coding/file_name_utils.hpp"
#include "coding/file_reader.hpp"
#include "coding/file_writer.hpp"
#include "coding/sha1.hpp"
#include "coding/zip_reader.hpp"

#include "base/scope_guard.hpp"
#include "base/string_utils.hpp"

namespace
{
void ValidateKmlData(std::unique_ptr<kml::FileData> & data)
{
  if (!data)
    return;

  for (auto & t : data->m_tracksData)
  {
    if (t.m_layers.empty())
      t.m_layers.emplace_back(kml::KmlParser::GetDefaultTrackLayer());
  }
}
}  // namespace

std::string const kKmzExtension = ".kmz";
std::string const kKmlExtension = ".kml";
std::string const kKmbExtension = ".kmb";

std::unique_ptr<kml::FileData> LoadKmlFile(std::string const & file, KmlFileType fileType)
{
  std::unique_ptr<kml::FileData> kmlData;
  try
  {
    kmlData = LoadKmlData(FileReader(file, true /* withExceptions */), fileType);
  }
  catch (std::exception const & e)
  {
    LOG(LWARNING, ("KML", fileType, "loading failure:", e.what()));
    kmlData.reset();
  }
  if (kmlData == nullptr)
    LOG(LWARNING, ("Loading bookmarks failed, file", file));
  return kmlData;
}

std::unique_ptr<kml::FileData> LoadKmzFile(std::string const & file, std::string & kmlHash)
{
  ZipFileReader::FileListT files;
  ZipFileReader::FilesList(file, files);
  if (files.empty())
    return nullptr;
  
  auto fileName = files.front().first;
  for (auto const & file : files)
  {
    if (strings::MakeLowerCase(my::GetFileExtension(file.first)) == kKmlExtension)
    {
      fileName = file.first;
      break;
    }
  }
  std::string const unarchievedPath = file + ".raw";
  MY_SCOPE_GUARD(fileGuard, bind(&FileWriter::DeleteFileX, unarchievedPath));
  ZipFileReader::UnzipFile(file, fileName, unarchievedPath);
  if (!GetPlatform().IsFileExistsByFullPath(unarchievedPath))
    return nullptr;
  
  kmlHash = coding::SHA1::CalculateBase64(unarchievedPath);
  return LoadKmlFile(unarchievedPath, KmlFileType::Text);
}

std::unique_ptr<kml::FileData> LoadKmlData(Reader const & reader, KmlFileType fileType)
{
  auto data = std::make_unique<kml::FileData>();
  try
  {
    if (fileType == KmlFileType::Binary)
    {
      kml::binary::DeserializerKml des(*data);
      des.Deserialize(reader);
    }
    else if (fileType == KmlFileType::Text)
    {
      kml::DeserializerKml des(*data);
      des.Deserialize(reader);
    }
    else
    {
      CHECK(false, ("Not supported KmlFileType"));
    }
    ValidateKmlData(data);
  }
  catch (Reader::Exception const & e)
  {
    LOG(LWARNING, ("KML", fileType, "reading failure:", e.what()));
    return nullptr;
  }
  catch (kml::binary::DeserializerKml::DeserializeException const & e)
  {
    LOG(LWARNING, ("KML", fileType, "deserialization failure:", e.what()));
    return nullptr;
  }
  catch (kml::DeserializerKml::DeserializeException const & e)
  {
    LOG(LWARNING, ("KML", fileType, "deserialization failure:", e.what()));
    return nullptr;
  }
  catch (std::exception const & e)
  {
    LOG(LWARNING, ("KML", fileType, "loading failure:", e.what()));
    return nullptr;
  }
  return data;
}

bool SaveKmlFile(kml::FileData & kmlData, std::string const & file, KmlFileType fileType)
{
  bool success;
  try
  {
    FileWriter writer(file);
    success = SaveKmlData(kmlData, writer, fileType);
  }
  catch (std::exception const & e)
  {
    LOG(LWARNING, ("KML", fileType, "saving failure:", e.what()));
    success = false;
  }
  if (!success)
    LOG(LWARNING, ("Saving bookmarks failed, file", file));
  return success;
}

bool SaveKmlData(kml::FileData & kmlData, Writer & writer, KmlFileType fileType)
{
  try
  {
    if (fileType == KmlFileType::Binary)
    {
      kml::binary::SerializerKml ser(kmlData);
      ser.Serialize(writer);
    }
    else if (fileType == KmlFileType::Text)
    {
      kml::SerializerKml ser(kmlData);
      ser.Serialize(writer);
    }
    else
    {
      CHECK(false, ("Not supported KmlFileType"));
    }
  }
  catch (Writer::Exception const & e)
  {
    LOG(LWARNING, ("KML", fileType, "writing failure:", e.what()));
    return false;
  }
  catch (std::exception const & e)
  {
    LOG(LWARNING, ("KML", fileType, "serialization failure:", e.what()));
    return false;
  }
  return true;
}

void ResetIds(kml::FileData & kmlData)
{
  kmlData.m_categoryData.m_id = kml::kInvalidMarkGroupId;
  for (auto & bmData : kmlData.m_bookmarksData)
    bmData.m_id = kml::kInvalidMarkId;
  for (auto & trackData : kmlData.m_tracksData)
    trackData.m_id = kml::kInvalidTrackId;
}

void SaveFeatureTypes(feature::TypesHolder const & types, kml::BookmarkData & bmData)
{
  auto const & c = classif();
  feature::TypesHolder copy(types);
  copy.SortBySpec();
  bmData.m_featureTypes.reserve(copy.Size());
  for (auto it = copy.begin(); it != copy.end(); ++it)
  {
    bmData.m_featureTypes.push_back(c.GetIndexForType(*it));
  }
}

std::string GetPreferredBookmarkStr(kml::LocalizableString const & name)
{
  if (name.size() == 1)
    return name.begin()->second;

  StringUtf8Multilang nameMultilang;
  for (auto const & pair : name)
    nameMultilang.AddString(pair.first, pair.second);

  auto const deviceLang = StringUtf8Multilang::GetLangIndex(languages::GetCurrentNorm());

  std::string preferredName;
  if (feature::GetPreferredName(nameMultilang, deviceLang, preferredName))
    return preferredName;

  return {};
}

std::string GetPreferredBookmarkStr(kml::LocalizableString const & name, feature::RegionData const & regionData)
{
  if (name.size() == 1)
    return name.begin()->second;

  StringUtf8Multilang nameMultilang;
  for (auto const & pair : name)
    nameMultilang.AddString(pair.first, pair.second);

  auto const deviceLang = StringUtf8Multilang::GetLangIndex(languages::GetCurrentNorm());

  std::string preferredName;
  feature::GetReadableName(regionData, nameMultilang, deviceLang, false /* allowTranslit */, preferredName);
  return preferredName;
}

std::string GetLocalizedBookmarkType(std::vector<uint32_t> const & types)
{
  if (types.empty())
    return {};

  auto const & c = classif();
  auto const type = c.GetTypeForIndex(types.front());
  CategoriesHolder const & categories = GetDefaultCategories();
  return categories.GetReadableFeatureType(type, categories.MapLocaleToInteger(languages::GetCurrentOrig()));
}

std::string GetPreferredBookmarkName(kml::BookmarkData const & bmData)
{
  std::string name = GetPreferredBookmarkStr(bmData.m_customName);
  if (name.empty())
    name = GetPreferredBookmarkStr(bmData.m_name);
  if (name.empty())
    name = GetLocalizedBookmarkType(bmData.m_featureTypes);
  return name;
}

bool FromCatalog(kml::FileData const & kmlData)
{
  return FromCatalog(kmlData.m_categoryData, kmlData.m_serverId);
}

bool FromCatalog(kml::CategoryData const & categoryData, std::string const & serverId)
{
  return !serverId.empty() && categoryData.m_accessRules == kml::AccessRules::Public;
}
