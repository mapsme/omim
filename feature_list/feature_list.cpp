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

#include "platform/local_country_file_utils.hpp"
#include "platform/platform.hpp"

#include "search/engine.hpp"
#include "search/reverse_geocoder.hpp"
#include "search/search_quality/helpers.hpp"

#include "storage/index.hpp"
#include "storage/storage.hpp"

#include <algorithm>
#include <iostream>

class ClosestPoint
{
  m2::PointD const & m_center;
  m2::PointD m_best;
  double m_distance = numeric_limits<double>::max();

public:
  ClosestPoint(m2::PointD const & center) : m_center(center), m_best(0, 0) {}
  m2::PointD GetBest() const { return m_best; }

  void operator()(m2::PointD const & point)
  {
    double distance = m_center.SquareLength(point);
    if (distance < m_distance)
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
    f.ForEachTriangle([&closest](m2::PointD const & p1, m2::PointD const & p2,
                                 m2::PointD const & p3) { closest((p1 + p2 + p3) / 3); },
                      FeatureType::BEST_GEOMETRY);
  }
  else
  {
    f.ForEachPoint(closest, FeatureType::BEST_GEOMETRY);
  }
  return closest.GetBest();
}

size_t const kLangCount = StringUtf8Multilang::GetSupportedLanguages().size();

set<std::string> const kPoiTypes = {"amenity",  "shop",    "tourism",  "leisure",   "sport",
                               "craft",    "place",   "man_made", "emergency", "office",
                               "historic", "railway", "highway",  "aeroway"};

std::string GetReadableType(FeatureType const & f)
{
  uint32_t result = 0;
  f.ForEachType([&result](uint32_t type)
  {
    std::string fullName = classif().GetFullObjectName(type);
    auto pos = fullName.find("|");
    if (pos != std::string::npos)
      fullName = fullName.substr(0, pos);
    if (kPoiTypes.find(fullName) != kPoiTypes.end())
      result = type;
  });
  return result == 0 ? std::string() : classif().GetReadableObjectName(result);
}

std::string GetWheelchairType(FeatureType const & f)
{
  static const uint32_t wheelchair = classif().GetTypeByPath({"wheelchair"});
  std::string result;
  f.ForEachType([&result](uint32_t type)
  {
    uint32_t truncated = type;
    ftype::TruncValue(truncated, 1);
    if (truncated == wheelchair)
    {
      std::string fullName = classif().GetReadableObjectName(type);
      auto pos = fullName.find("-");
      if (pos != std::string::npos)
        result = fullName.substr(pos + 1);
    }
  });
  return result;
}

std::string BuildUniqueId(ms::LatLon const & coords, std::string const & name)
{
  ostringstream ss;
  ss << strings::to_string_with_digits_after_comma(coords.lat, 6) << ','
     << strings::to_string_with_digits_after_comma(coords.lon, 6) << ','
     << name;
  uint32_t hash = 0;
  for (char const c : ss.str())
    hash = hash * 101 + c;
  return strings::to_string(hash);
}

void AppendNames(FeatureType const & f, vector<std::string> & columns)
{
  vector<std::string> names(kLangCount);
  f.GetNames().ForEach([&names](int8_t code, std::string const & name) -> bool
  {
    names[code] = std::string(name);
    return true;
  });
  columns.insert(columns.end(), next(names.begin()), names.end());
}

void PrintAsCSV(vector<std::string> const & columns, char const delimiter, std::ostream & out)
{
  bool first = true;
  for (std::string value : columns)
  {
    // Newlines are hard to process, replace them with spaces. And trim the string.
    std::replace(value.begin(), value.end(), '\r', ' ');
    std::replace(value.begin(), value.end(), '\n', ' ');
    strings::Trim(value);

    if (first)
      first = false;
    else
      out << delimiter;
    bool needsQuotes = value.find('"') != std::string::npos || value.find(delimiter) != std::string::npos;
    if (!needsQuotes)
    {
      out << value;
    }
    else
    {
      size_t pos = 0;
      while ((pos = value.find('"', pos)) != std::string::npos)
      {
        value.insert(pos, 1, '"');
        pos += 2;
      }
      out << '"' << value << '"';
    }
  }
  out << endl;
}

class Processor
{
  search::ReverseGeocoder m_geocoder;
  my::Cancellable m_cancellable;
  search::VillagesCache m_villagesCache;
  search::LocalityFinder m_finder;

public:
  Processor(Index const & index)
    : m_geocoder(index), m_villagesCache(m_cancellable), m_finder(index, m_villagesCache)
  {
  }

  void ClearCache() { m_villagesCache.Clear(); }

  void operator()(FeatureType const & f, uint32_t const & id) { Process(f); }

  void Process(FeatureType const & f)
  {
    f.ParseBeforeStatistic();
    std::string const & category = GetReadableType(f);
    if (!f.HasName() || f.GetFeatureType() == feature::GEOM_LINE || category.empty())
      return;
    m2::PointD const & center = FindCenter(f);
    ms::LatLon const & ll = MercatorBounds::ToLatLon(center);
    osm::MapObject obj;
    obj.SetFromFeatureType(f);

    std::string city;
    m_finder.GetLocality(center, city);

    std::string const & mwmName = f.GetID().GetMwmName();
    std::string name, secondary;
    f.GetPreferredNames(name, secondary);
    std::string const & uid = BuildUniqueId(ll, name);
    std::string const & lat = strings::to_string_with_digits_after_comma(ll.lat, 6);
    std::string const & lon = strings::to_string_with_digits_after_comma(ll.lon, 6);
    search::ReverseGeocoder::Address addr;
    std::string addrStreet = "";
    std::string addrHouse = "";
    if (m_geocoder.GetExactAddress(f, addr))
    {
      addrStreet = addr.GetStreetName();
      addrHouse = addr.GetHouseNumber();
    }
    std::string const & phone = f.GetMetadata().Get(feature::Metadata::FMD_PHONE_NUMBER);
    std::string const & website = f.GetMetadata().Get(feature::Metadata::FMD_WEBSITE);
    std::string cuisine = f.GetMetadata().Get(feature::Metadata::FMD_CUISINE);
    std::replace(cuisine.begin(), cuisine.end(), ';', ',');
    std::string const & stars = f.GetMetadata().Get(feature::Metadata::FMD_STARS);
    std::string const & operatr = f.GetMetadata().Get(feature::Metadata::FMD_OPERATOR);
    std::string const & internet = f.GetMetadata().Get(feature::Metadata::FMD_INTERNET);
    std::string const & denomination = f.GetMetadata().Get(feature::Metadata::FMD_DENOMINATION);
    std::string const & wheelchair = GetWheelchairType(f);
    std::string const & opening_hours = f.GetMetadata().Get(feature::Metadata::FMD_OPEN_HOURS);

    vector<std::string> columns = {uid,          lat,        lon,          mwmName,   category,
                              name,         city,       addrStreet,   addrHouse, phone,
                              website,      cuisine,    stars,        operatr,   internet,
                              denomination, wheelchair, opening_hours};
    AppendNames(f, columns);
    PrintAsCSV(columns, ';', std::cout);
  }
};

void PrintHeader()
{
  vector<std::string> columns = {"id",           "lat",        "lon",          "mwm",      "category",
                            "name",         "city",       "street",       "house",    "phone",
                            "website",      "cuisines",   "stars",        "operator", "internet",
                            "denomination", "wheelchair", "opening_hours"};
  // Append all supported name languages in order.
  for (uint8_t idx = 1; idx < kLangCount; idx++)
    columns.push_back("name_" + std::string(StringUtf8Multilang::GetLangByCode(idx)));
  PrintAsCSV(columns, ';', std::cout);
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
    LOG(LERROR, ("Usage:", argc == 1 ? argv[0] : "feature_list",
                 "<mwm_path> [<data_path>] [<mwm_prefix>]"));
    return 1;
  }

  Platform & pl = GetPlatform();
  pl.SetWritableDirForTests(argv[1]);

  std::string countriesFile = COUNTRIES_FILE;
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

  Index index;
  vector<platform::LocalCountryFile> mwms;
  platform::FindAllLocalMapsAndCleanup(numeric_limits<int64_t>::max() /* the latest version */,
                                       mwms);
  for (auto & mwm : mwms)
  {
    if (argc > 3 && !strings::StartsWith(mwm.GetCountryName(), argv[3]))
      continue;
    mwm.SyncWithDisk();
    auto const & p = index.RegisterMap(mwm);
    CHECK_EQUAL(MwmSet::RegResult::Success, p.second, ("Could not register map", mwm));
    MwmSet::MwmId const & id = p.first;
    CHECK(id.IsAlive(), ("Mwm is not alive?", mwm));
  }

  Processor doProcess(index);
  PrintHeader();
  vector<shared_ptr<MwmInfo>> mwmInfos;
  index.GetMwmsInfo(mwmInfos);
  for (auto const & mwmInfo : mwmInfos)
  {
    if (mwmInfo->GetType() != MwmInfo::COUNTRY)
      continue;
    LOG(LINFO, ("Processing", mwmInfo->GetCountryName()));
    MwmSet::MwmId mwmId(mwmInfo);
    Index::FeaturesLoaderGuard loader(index, mwmId);
    for (uint32_t ftIndex = 0; ftIndex < loader.GetNumFeatures(); ftIndex++)
    {
      FeatureType ft;
      if (loader.GetFeatureByIndex(static_cast<uint32_t>(ftIndex), ft))
        doProcess.Process(ft);
    }
    doProcess.ClearCache();
  }

  return 0;
}
