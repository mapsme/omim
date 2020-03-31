#include "search/search_quality/helpers.hpp"

#include "storage/country_info_getter.hpp"
#include "storage/country_tree.hpp"
#include "storage/storage.hpp"

#include "indexer/data_source.hpp"

#include "platform/local_country_file.hpp"
#include "platform/local_country_file_utils.hpp"
#include "platform/platform.hpp"

#include "coding/reader.hpp"
#include "coding/reader_wrapper.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"
#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include "std/target_os.hpp"

#include <fstream>
#include <limits>
#include <utility>

#include "defines.hpp"

#include "3party/jansson/myjansson.hpp"

using namespace std;

namespace
{
uint64_t ReadVersionFromHeader(platform::LocalCountryFile const & mwm)
{
  vector<string> const kSpecialFiles = {WORLD_FILE_NAME, WORLD_COASTS_FILE_NAME};
  for (auto const & name : kSpecialFiles)
  {
    if (mwm.GetCountryName() == name)
      return mwm.GetVersion();
  }

  ModelReaderPtr reader =
      FilesContainerR(mwm.GetPath(MapFileType::Map)).GetReader(VERSION_FILE_TAG);
  ReaderSrc src(reader.GetPtr());

  version::MwmVersion version;
  version::ReadVersion(src, version);
  return version.GetVersion();
}
}  // namespace

namespace search
{
namespace search_quality
{
void CheckLocale()
{
  string const kJson = "{\"coord\":123.456}";
  string const kErrorMsg = "Bad locale. Consider setting LC_ALL=C";

  double coord;
  {
    base::Json root(kJson.c_str());
    FromJSONObject(root.get(), "coord", coord);
  }

  string line;
  {
    auto root = base::NewJSONObject();
    ToJSONObject(*root, "coord", coord);

    unique_ptr<char, JSONFreeDeleter> buffer(
        json_dumps(root.get(), JSON_COMPACT | JSON_ENSURE_ASCII));

    line.append(buffer.get());
  }

  CHECK_EQUAL(line, kJson, (kErrorMsg));

  {
    string const kTest = "123.456";
    double value;
    VERIFY(strings::to_double(kTest, value), (kTest));
    CHECK_EQUAL(strings::to_string(value), kTest, (kErrorMsg));
  }
}

void ReadStringsFromFile(string const & path, vector<string> & result)
{
  ifstream stream(path.c_str());
  CHECK(stream.is_open(), ("Can't open", path));

  string s;
  while (getline(stream, s))
  {
    strings::Trim(s);
    if (!s.empty())
      result.emplace_back(s);
  }
}

void SetPlatformDirs(string const & dataPath, string const & mwmPath)
{
  Platform & platform = GetPlatform();

  if (!dataPath.empty())
    platform.SetResourceDir(dataPath);

  if (!mwmPath.empty())
    platform.SetWritableDirForTests(mwmPath);

  LOG(LINFO, ("writable dir =", platform.WritableDir()));
  LOG(LINFO, ("resources dir =", platform.ResourcesDir()));
}

void InitViewport(string viewportName, m2::RectD & viewport)
{
  map<string, m2::RectD> const kViewports = {
      {"default", m2::RectD(m2::PointD(0.0, 0.0), m2::PointD(1.0, 1.0))},
      {"moscow", mercator::RectByCenterLatLonAndSizeInMeters(55.7, 37.7, 5000)},
      {"london", mercator::RectByCenterLatLonAndSizeInMeters(51.5, 0.0, 5000)},
      {"zurich", mercator::RectByCenterLatLonAndSizeInMeters(47.4, 8.5, 5000)}};

  auto it = kViewports.find(viewportName);
  if (it == kViewports.end())
  {
    LOG(LINFO, ("Unknown viewport name:", viewportName, "; setting to default"));
    viewportName = "default";
    it = kViewports.find(viewportName);
  }
  CHECK(it != kViewports.end(), ());
  viewport = it->second;
  LOG(LINFO, ("Viewport is set to:", viewportName, DebugPrint(viewport)));
}

void InitDataSource(FrozenDataSource & dataSource, string const & mwmListPath)
{
  vector<platform::LocalCountryFile> mwms;
  if (!mwmListPath.empty())
  {
    vector<string> availableMwms;
    ReadStringsFromFile(mwmListPath, availableMwms);
    for (auto const & countryName : availableMwms)
      mwms.emplace_back(GetPlatform().WritableDir(), platform::CountryFile(countryName), 0);
  }
  else
  {
    platform::FindAllLocalMapsAndCleanup(numeric_limits<int64_t>::max() /* the latest version */,
                                         mwms);
  }

  LOG(LINFO, ("Initializing the data source with the following mwms:"));
  for (auto & mwm : mwms)
  {
    mwm.SyncWithDisk();
    LOG(LINFO, (mwm.GetCountryName(), ReadVersionFromHeader(mwm)));
    dataSource.RegisterMap(mwm);
  }
  LOG(LINFO, ());
}

void InitStorageData(storage::Affiliations & affiliations,
                     storage::CountryNameSynonyms & countryNameSynonyms)
{
  auto const countriesFile = base::JoinPath(GetPlatform().ResourcesDir(), COUNTRIES_FILE);

  storage::CountryTree countries;
  storage::MwmTopCityGeoIds mwmTopCityGeoIds;
  storage::MwmTopCountryGeoIds mwmTopCountryGeoIds;
  auto const rv =
      storage::LoadCountriesFromFile(COUNTRIES_FILE, countries, affiliations, countryNameSynonyms,
                                     mwmTopCityGeoIds, mwmTopCountryGeoIds);
  CHECK(rv != -1, ("Can't load countries from:", countriesFile));
}

unique_ptr<search::tests_support::TestSearchEngine> InitSearchEngine(
    DataSource & dataSource, storage::Affiliations const & affiliations, string const & locale,
    size_t numThreads)
{
  search::Engine::Params params;
  params.m_locale = locale;
  params.m_numThreads = base::checked_cast<size_t>(numThreads);

  auto infoGetter = storage::CountryInfoReader::CreateCountryInfoGetter(GetPlatform());
  infoGetter->SetAffiliations(&affiliations);

  return make_unique<search::tests_support::TestSearchEngine>(dataSource, move(infoGetter), params);
}
}  // namespace search_quality
}  // namespace search
