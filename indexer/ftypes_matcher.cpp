#include "indexer/ftypes_matcher.hpp"

#include "indexer/feature.hpp"
#include "indexer/feature_data.hpp"
#include "indexer/classificator.hpp"

#include "std/map.hpp"
#include "std/sstream.hpp"
#include "std/utility.hpp"

namespace
{
class HighwayClasses
{
  map<uint32_t, ftypes::HighwayClass> m_map;

public:
  HighwayClasses()
  {
    auto const & c = classif();
    m_map[c.GetTypeByPath({"highway", "motorway"})] = ftypes::HighwayClass::Trunk;
    m_map[c.GetTypeByPath({"highway", "motorway_link"})] = ftypes::HighwayClass::Trunk;
    m_map[c.GetTypeByPath({"highway", "trunk"})] = ftypes::HighwayClass::Trunk;
    m_map[c.GetTypeByPath({"highway", "trunk_link"})] = ftypes::HighwayClass::Trunk;
    m_map[c.GetTypeByPath({"route", "ferry"})] = ftypes::HighwayClass::Trunk;

    m_map[c.GetTypeByPath({"highway", "primary"})] = ftypes::HighwayClass::Primary;
    m_map[c.GetTypeByPath({"highway", "primary_link"})] = ftypes::HighwayClass::Primary;

    m_map[c.GetTypeByPath({"highway", "secondary"})] = ftypes::HighwayClass::Secondary;
    m_map[c.GetTypeByPath({"highway", "secondary_link"})] = ftypes::HighwayClass::Secondary;

    m_map[c.GetTypeByPath({"highway", "tertiary"})] = ftypes::HighwayClass::Tertiary;
    m_map[c.GetTypeByPath({"highway", "tertiary_link"})] = ftypes::HighwayClass::Tertiary;

    m_map[c.GetTypeByPath({"highway", "unclassified"})] = ftypes::HighwayClass::LivingStreet;
    m_map[c.GetTypeByPath({"highway", "residential"})] = ftypes::HighwayClass::LivingStreet;
    m_map[c.GetTypeByPath({"highway", "living_street"})] = ftypes::HighwayClass::LivingStreet;
    m_map[c.GetTypeByPath({"highway", "road"})] = ftypes::HighwayClass::LivingStreet;

    m_map[c.GetTypeByPath({"highway", "service"})] = ftypes::HighwayClass::Service;
    m_map[c.GetTypeByPath({"highway", "track"})] = ftypes::HighwayClass::Service;

    m_map[c.GetTypeByPath({"highway", "pedestrian"})] = ftypes::HighwayClass::Pedestrian;
    m_map[c.GetTypeByPath({"highway", "footway"})] = ftypes::HighwayClass::Pedestrian;
    m_map[c.GetTypeByPath({"highway", "bridleway"})] = ftypes::HighwayClass::Pedestrian;
    m_map[c.GetTypeByPath({"highway", "steps"})] = ftypes::HighwayClass::Pedestrian;
    m_map[c.GetTypeByPath({"highway", "cycleway"})] = ftypes::HighwayClass::Pedestrian;
    m_map[c.GetTypeByPath({"highway", "path"})] = ftypes::HighwayClass::Pedestrian;
  }

  ftypes::HighwayClass Get(uint32_t t) const
  {
    auto const it = m_map.find(t);
    if (it == m_map.cend())
      return ftypes::HighwayClass::Error;
    return it->second;
  }
};

char const * HighwayClassToString(ftypes::HighwayClass const cls)
{
  switch (cls)
  {
  case ftypes::HighwayClass::Undefined: return "Undefined";
  case ftypes::HighwayClass::Error: return "Error";
  case ftypes::HighwayClass::Trunk: return "Trunk";
  case ftypes::HighwayClass::Primary: return "Primary";
  case ftypes::HighwayClass::Secondary: return "Secondary";
  case ftypes::HighwayClass::Tertiary: return "Tertiary";
  case ftypes::HighwayClass::LivingStreet: return "LivingStreet";
  case ftypes::HighwayClass::Service: return "Service";
  case ftypes::HighwayClass::Pedestrian: return "Pedestrian";
  case ftypes::HighwayClass::Count: return "Count";
  }
  ASSERT(false, ());
  return "";
}
}  // namespace

namespace ftypes
{
string DebugPrint(HighwayClass const cls)
{
  stringstream out;
  out << "[ " << HighwayClassToString(cls) << " ]";
  return out.str();
}

HighwayClass GetHighwayClass(feature::TypesHolder const & types)
{
  uint8_t constexpr kTruncLevel = 2;
  static HighwayClasses highwayClasses;

  for (auto t : types)
  {
    ftype::TruncValue(t, kTruncLevel);
    HighwayClass const hc = highwayClasses.Get(t);
    if (hc != HighwayClass::Error)
      return hc;
  }

  return HighwayClass::Error;
}

uint32_t BaseChecker::PrepareToMatch(uint32_t type, uint8_t level)
{
  ftype::TruncValue(type, level);
  return type;
}

bool BaseChecker::IsMatched(uint32_t type) const
{
  return (find(m_types.begin(), m_types.end(), PrepareToMatch(type, m_level)) != m_types.end());
}

bool BaseChecker::operator()(feature::TypesHolder const & types) const
{
  for (uint32_t t : types)
    if (IsMatched(t))
      return true;

  return false;
}

bool BaseChecker::operator()(FeatureType const & ft) const
{
  return this->operator()(feature::TypesHolder(ft));
}

bool BaseChecker::operator()(vector<uint32_t> const & types) const
{
  for (size_t i = 0; i < types.size(); ++i)
  {
    if (IsMatched(types[i]))
      return true;
  }
  return false;
}

IsPeakChecker::IsPeakChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"natural", "peak"}));
}

IsPeakChecker const & IsPeakChecker::Instance()
{
  static IsPeakChecker const inst;
  return inst;
}

IsATMChecker::IsATMChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"amenity", "atm"}));
}

IsATMChecker const & IsATMChecker::Instance()
{
  static IsATMChecker const inst;
  return inst;
}

IsSpeedCamChecker::IsSpeedCamChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"highway", "speed_camera"}));
}

// static
IsSpeedCamChecker const & IsSpeedCamChecker::Instance()
{
  static IsSpeedCamChecker const instance;
  return instance;
}

IsFuelStationChecker::IsFuelStationChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"amenity", "fuel"}));
}

IsFuelStationChecker const & IsFuelStationChecker::Instance()
{
  static IsFuelStationChecker const inst;
  return inst;
}

IsRailwayStationChecker::IsRailwayStationChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"railway", "station"}));
}

IsRailwayStationChecker const & IsRailwayStationChecker::Instance()
{
  static IsRailwayStationChecker const inst;
  return inst;
}

IsStreetChecker::IsStreetChecker()
{
  // TODO (@y, @m, @vng): this list must be up-to-date with
  // data/categories.txt, so, it's worth it to generate or parse it
  // from that file.
  Classificator const & c = classif();
  char const * arr[][2] = {{"highway", "living_street"},
                           {"highway", "footway"},
                           {"highway", "motorway"},
                           {"highway", "motorway_link"},
                           {"highway", "path"},
                           {"highway", "pedestrian"},
                           {"highway", "primary"},
                           {"highway", "primary_link"},
                           {"highway", "residential"},
                           {"highway", "road"},
                           {"highway", "secondary"},
                           {"highway", "secondary_link"},
                           {"highway", "service"},
                           {"highway", "tertiary"},
                           {"highway", "tertiary_link"},
                           {"highway", "track"},
                           {"highway", "trunk"},
                           {"highway", "trunk_link"},
                           {"highway", "unclassified"}};
  for (auto const & p : arr)
    m_types.push_back(c.GetTypeByPath({p[0], p[1]}));
}

IsStreetChecker const & IsStreetChecker::Instance()
{
  static IsStreetChecker const inst;
  return inst;
}

IsAddressObjectChecker::IsAddressObjectChecker() : BaseChecker(1 /* level */)
{
  auto const paths = {"building", "amenity", "shop", "tourism", "historic", "office", "craft"};

  Classificator const & c = classif();
  for (auto const & p : paths)
    m_types.push_back(c.GetTypeByPath({p}));
}

IsAddressObjectChecker const & IsAddressObjectChecker::Instance()
{
  static IsAddressObjectChecker const inst;
  return inst;
}

IsVillageChecker::IsVillageChecker()
{
  // TODO (@y, @m, @vng): this list must be up-to-date with
  // data/categories.txt, so, it's worth it to generate or parse it
  // from that file.
  Classificator const & c = classif();
  char const * arr[][2] = {{"place", "village"}, {"place", "hamlet"}};

  for (auto const & p : arr)
    m_types.push_back(c.GetTypeByPath({p[0], p[1]}));
}

IsVillageChecker const & IsVillageChecker::Instance()
{
  static IsVillageChecker const inst;
  return inst;
}

IsOneWayChecker::IsOneWayChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"hwtag", "oneway"}));
}

IsOneWayChecker const & IsOneWayChecker::Instance()
{
  static IsOneWayChecker const inst;
  return inst;
}

IsRoundAboutChecker::IsRoundAboutChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"junction", "roundabout"}));
}

IsRoundAboutChecker const & IsRoundAboutChecker::Instance()
{
  static IsRoundAboutChecker const inst;
  return inst;
}

IsLinkChecker::IsLinkChecker()
{
  Classificator const & c = classif();
  char const * arr[][2] = {{"highway", "motorway_link"},
                           {"highway", "trunk_link"},
                           {"highway", "primary_link"},
                           {"highway", "secondary_link"},
                           {"highway", "tertiary_link"}};

  for (size_t i = 0; i < ARRAY_SIZE(arr); ++i)
    m_types.push_back(c.GetTypeByPath(vector<string>(arr[i], arr[i] + 2)));
}

IsLinkChecker const & IsLinkChecker::Instance()
{
  static IsLinkChecker const inst;
  return inst;
}

IsBuildingChecker::IsBuildingChecker() : BaseChecker(1 /* level */)
{
  m_types.push_back(classif().GetTypeByPath({"building"}));
}

IsBuildingChecker const & IsBuildingChecker::Instance()
{
  static IsBuildingChecker const inst;
  return inst;
}

IsLocalityChecker::IsLocalityChecker()
{
  Classificator const & c = classif();

  // Note! The order should be equal with constants in Type enum (add other villages to the end).
  char const * arr[][2] = {
    { "place", "country" },
    { "place", "state" },
    { "place", "city" },
    { "place", "town" },
    { "place", "village" },
    { "place", "hamlet" }
  };

  for (size_t i = 0; i < ARRAY_SIZE(arr); ++i)
    m_types.push_back(c.GetTypeByPath(vector<string>(arr[i], arr[i] + 2)));
}

IsBuildingPartChecker::IsBuildingPartChecker() : BaseChecker(1 /* level */)
{
  m_types.push_back(classif().GetTypeByPath({"building:part"}));
}

IsBuildingPartChecker const & IsBuildingPartChecker::Instance()
{
  static IsBuildingPartChecker const inst;
  return inst;
}

IsBridgeChecker::IsBridgeChecker() : BaseChecker(3 /* level */) {}
IsBridgeChecker const & IsBridgeChecker::Instance()
{
  static IsBridgeChecker const inst;
  return inst;
}

bool IsBridgeChecker::IsMatched(uint32_t type) const
{
  return IsTypeConformed(type, {"highway", "*", "bridge"});
}

IsTunnelChecker::IsTunnelChecker() : BaseChecker(3 /* level */) {}
IsTunnelChecker const & IsTunnelChecker::Instance()
{
  static IsTunnelChecker const inst;
  return inst;
}

bool IsTunnelChecker::IsMatched(uint32_t type) const
{
  return IsTypeConformed(type, {"highway", "*", "tunnel"});
}

Type IsLocalityChecker::GetType(uint32_t t) const
{
  ftype::TruncValue(t, 2);

  size_t j = COUNTRY;
  for (; j < LOCALITY_COUNT; ++j)
    if (t == m_types[j])
      return static_cast<Type>(j);

  for (; j < m_types.size(); ++j)
    if (t == m_types[j])
      return VILLAGE;

  return NONE;
}

Type IsLocalityChecker::GetType(feature::TypesHolder const & types) const
{
  for (uint32_t t : types)
  {
    Type const type = GetType(t);
    if (type != NONE)
      return type;
  }
  return NONE;
}

Type IsLocalityChecker::GetType(FeatureType const & f) const
{
  feature::TypesHolder types(f);
  return GetType(types);
}

IsLocalityChecker const & IsLocalityChecker::Instance()
{
  static IsLocalityChecker const inst;
  return inst;
}

IsBookingChecker::IsBookingChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"sponsored", "booking"}));
}

IsBookingChecker const & IsBookingChecker::Instance()
{
  static IsBookingChecker const inst;
  return inst;
}

IsHotelChecker::IsHotelChecker()
{
  Classificator const & c = classif();
  for (auto const & tag : GetHotelTags())
    m_types.push_back(c.GetTypeByPath({"tourism", tag}));
}

IsHotelChecker const & IsHotelChecker::Instance()
{
  static IsHotelChecker const inst;
  return inst;
}

vector<string> const & IsHotelChecker::GetHotelTags()
{
  static vector<string> hotelTags = {"hotel",       "apartment", "camp_site", "chalet",
                                     "guest_house", "hostel",    "motel",     "resort"};
  return hotelTags;
}

IsWifiChecker::IsWifiChecker()
{
  m_types.push_back(classif().GetTypeByPath({"internet_access", "wlan"}));
}

IsWifiChecker const & IsWifiChecker::Instance()
{
  static IsWifiChecker const instance;
  return instance;
}

IsFoodChecker:: IsFoodChecker()
{
  Classificator const & c = classif();
  char const * const paths[][2] = {
    {"amenity", "cafe"},
    {"amenity", "bar"},
    {"amenity", "pub"},
    {"amenity", "fast_food"},
    {"amenity", "restaurant"}
  };
  for (auto const & path : paths)
    m_types.push_back(c.GetTypeByPath({path[0], path[1]}));
}

IsFoodChecker const & IsFoodChecker::Instance()
{
  static const IsFoodChecker instance;
  return instance;
}

IsOpentableChecker::IsOpentableChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"sponsored", "opentable"}));
}

IsOpentableChecker const & IsOpentableChecker::Instance()
{
  static IsOpentableChecker const inst;
  return inst;
}

uint32_t GetPopulation(FeatureType const & ft)
{
  uint32_t population = ft.GetPopulation();

  if (population < 10)
  {
    switch (IsLocalityChecker::Instance().GetType(ft))
    {
    case CITY:
    case TOWN:
      population = 10000;
      break;
    case VILLAGE:
      population = 100;
      break;
    default:
      population = 0;
    }
  }

  return population;
}

double GetRadiusByPopulation(uint32_t p)
{
  return pow(static_cast<double>(p), 0.277778) * 550.0;
}

uint32_t GetPopulationByRadius(double r)
{
  return my::rounds(pow(r / 550.0, 3.6));
}

bool IsTypeConformed(uint32_t type, StringIL const & path)
{
  ClassifObject const * p = classif().GetRoot();
  ASSERT(p, ());

  uint8_t val = 0, i = 0;
  for (char const * s : path)
  {
    if (!ftype::GetValue(type, i, val))
      return false;

    p = p->GetObject(val);
    if (p == 0)
      return false;

    if (p->GetName() != s && strcmp(s, "*") != 0)
      return false;

    ++i;
  }
  return true;
}
}  // namespace ftypes
