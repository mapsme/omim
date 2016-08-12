#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include "coding/file_name_utils.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"

#include "indexer/classificator.hpp"
#include "indexer/classificator_loader.hpp"
#include "indexer/feature.hpp"
#include "indexer/feature_processor.hpp"
#include "indexer/map_object.hpp"
#include "indexer/map_style_reader.hpp"

#include "platform/platform.hpp"
#include "platform/local_country_file_utils.hpp"

#include "search/engine.hpp"
#include "search/search_quality/helpers.hpp"
#include "search/reverse_geocoder.hpp"

#include "std/iostream.hpp"

#include "storage/index.hpp"
#include "storage/storage.hpp"

class ClosestPoint
{
  m2::PointD const & m_center;
  m2::PointD m_best;
  double m_distance = -1;

public:
  ClosestPoint(m2::PointD const & center) : m_center(center) {}

  m2::PointD GetBest() const { return m_best; }

  void operator() (m2::PointD const & point)
  {
    double distance = m_center.SquareLength(point);
    if (m_distance < 0 || distance < m_distance)
    {
      m_distance = distance;
      m_best = point;
    }
  }
};

m2::PointD FindCenter(FeatureType const & f)
{
  ClosestPoint closest(f.GetLimitRect(FeatureType::BEST_GEOMETRY).Center());
  if (f.GetFeatureType() == feature::GEOM_AREA)
  {
    f.ForEachTriangle([&closest](m2::PointD const & p1, m2::PointD const & p2, m2::PointD const & p3)
    {
      closest((p1 + p2 + p3) / 3);
    }, FeatureType::BEST_GEOMETRY);
  }
  else
  {
    f.ForEachPoint(closest, FeatureType::BEST_GEOMETRY);
  }
  return closest.GetBest();
}

string GetReadableType(FeatureType const & f)
{
  uint32_t result = 0;
  f.ForEachType([&result](uint32_t type)
  {
    string fullName = classif().GetFullObjectName(type);
    auto pos = fullName.find("|");
    if (pos != string::npos)
      fullName = fullName.substr(0, pos);
    if (fullName == "amenity" || fullName == "shop" || fullName == "tourism" ||
        fullName == "leisure" || fullName == "craft" || fullName == "place" ||
        fullName == "man_made" || fullName == "emergency" || fullName == "historic" ||
        fullName == "railway" || fullName == "highway" || fullName == "aeroway")
      result = type;
  });
  return result == 0 ? string() : classif().GetReadableObjectName(result);
}

string BuildUniqueId(ms::LatLon const & coords, string const & name)
{
  ostringstream ss;
  ss << strings::to_string_dac(coords.lat, 6) << ',' << strings::to_string_dac(coords.lon, 6) << ',' << name;
  uint32_t hash = 0;
  for (char const c : ss.str())
    hash = hash * 101 + c;
  return strings::to_string(hash);
}

void AppendNames(FeatureType const & f, vector<string> & columns)
{
  f.GetNames().ForEach([&columns](int8_t code, string const & name)
  {
    if (code != StringUtf8Multilang::kDefaultCode)
    {
      columns.push_back(StringUtf8Multilang::GetLangByCode(code));
      columns.push_back(name);
    }
  });
}

void PrintAsCSV(vector<string> const & columns, ostream & out, char delimiter = ',')
{
  bool first = true;
  for (string const & value : columns)
  {
    if (first)
      first = false;
    else
      out << delimiter;
    bool needsQuotes = value.find('"') != string::npos || value.find(delimiter) != string::npos;
    if (!needsQuotes)
    {
      out << value;
    }
    else
    {
      string quoted(value);
      size_t pos = 0;
      while ((pos = quoted.find('"', pos)) != string::npos)
      {
        quoted.insert(pos, 1, '"');
        pos += 2;
      }
      out << '"' << quoted << '"';
    }
  }
  out << endl;
}

class Processor
{
  search::ReverseGeocoder m_geocoder;
  search::LocalityFinder m_finder;

public:
  Processor(Index const & index) : m_geocoder(index), m_finder(index) {
  }

  void operator() (FeatureType const & f, uint32_t const & id)
  {
    process(f);
  }

  void process(FeatureType const & f)
  {
    f.ParseBeforeStatistic();
    string category = GetReadableType(f);
    if (!f.HasName() || f.GetFeatureType() == feature::GEOM_LINE || category.empty())
      return;
    m2::PointD center = FindCenter(f);
    ms::LatLon ll = MercatorBounds::ToLatLon(center);
    osm::MapObject obj;
    obj.SetFromFeatureType(f);

    string city;
    m_finder.GetLocality(center, city);

    string mwmName = f.GetID().GetMwmName();
    string name, secondary;
    f.GetPreferredNames(name, secondary);
    string uid = BuildUniqueId(ll, name);
    string lat = strings::to_string_dac(ll.lat, 6);
    string lon = strings::to_string_dac(ll.lon, 6);
    search::ReverseGeocoder::Address addr;
    string address = m_geocoder.GetExactAddress(f, addr)? addr.GetStreetName() + ", " + addr.GetHouseNumber() : "";
    string phone = f.GetMetadata().Get(feature::Metadata::FMD_PHONE_NUMBER);
    string cuisine = strings::JoinStrings(obj.GetLocalizedCuisines(), ", ");
    string opening_hours = f.GetMetadata().Get(feature::Metadata::FMD_OPEN_HOURS);
    string website = "https://itunes.apple.com/app/id510623322?mt=8";

    vector<string> columns = {uid, lat, lon, mwmName, category, name, city, address, phone, cuisine, opening_hours, website};
    AppendNames(f, columns);
    PrintAsCSV(columns, cout, ';');
  }
};

void PrintHeader()
{
  vector<string> columns = {"id", "lat", "lon", "mwm", "category", "name", "city", "address", "phone", "cuisine", "opening_hours", "url"};
  PrintAsCSV(columns, cout, ';');
}

void DidDownload(storage::TCountryId const & /* countryId */,
                 shared_ptr<platform::LocalCountryFile> const & /* localFile */)
{
}

bool WillDelete(storage::TCountryId const & /* countryId */,
                shared_ptr<platform::LocalCountryFile> const & /* localFile */)
{
  return false;
}

int main(int argc, char ** argv)
{
  search::ChangeMaxNumberOfOpenFiles(search::kMaxOpenFiles);
  if (argc <= 1)
  {
    LOG(LERROR, ("Usage:", argc == 1 ? argv[0] : "feature_list", "<mwm_path> [<data_path>] [<mwm_prefix>]"));
    return 1;
  }

  Platform & pl = GetPlatform();
  pl.SetWritableDirForTests(argv[1]);

  string countriesFile = COUNTRIES_FILE;
  if (argc > 2)
  {
    pl.SetResourceDir(argv[2]);
    countriesFile = my::JoinFoldersToPath(argv[2], COUNTRIES_FILE);
  }

  storage::Storage storage(countriesFile, argv[1]);
  storage.Init(&DidDownload, &WillDelete);
  auto infoGetter = storage::CountryInfoReader::CreateCountryInfoReader(pl);
  infoGetter->InitAffiliationsInfo(&storage.GetAffiliations());

  GetStyleReader().SetCurrentStyle(MapStyleMerged);
  classificator::Load();
  classif().SortClassificator();
  PrepareRequiredTypes();

  Index engine;
  vector<platform::LocalCountryFile> mwms;
  platform::FindAllLocalMapsAndCleanup(numeric_limits<int64_t>::max() /* the latest version */,
                                       mwms);
  for (auto & mwm : mwms)
  {
    if (argc > 3 && !strings::StartsWith(mwm.GetCountryName(), argv[3]))
      continue;
    mwm.SyncWithDisk();
    auto const & p = engine.RegisterMap(mwm);
    CHECK_EQUAL(MwmSet::RegResult::Success, p.second, ("Could not register map", mwm));
    MwmSet::MwmId const & id = p.first;
    CHECK(id.IsAlive(), ("Mwm is not alive?", mwm));
  }

  Processor doProcess(engine);
  PrintHeader();
  vector<shared_ptr<MwmInfo>> mwmInfos;
  engine.GetMwmsInfo(mwmInfos);
  for (auto const & mwmInfo : mwmInfos)
  {
    if (mwmInfo->GetType() != MwmInfo::COUNTRY)
      continue;
    LOG(LINFO, ("Processing", mwmInfo->GetCountryName()));
    MwmSet::MwmId mwmId(mwmInfo);
    Index::FeaturesLoaderGuard loader(engine, mwmId);
    for (size_t ftIndex = 0; ftIndex < loader.GetNumFeatures(); ftIndex++)
    {
      FeatureType ft;
      if (loader.GetFeatureByIndex(ftIndex, ft))
        doProcess.process(ft);
    }
  }

  return 0;
}
