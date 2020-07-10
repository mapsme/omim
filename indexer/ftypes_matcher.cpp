#include "indexer/ftypes_matcher.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature.hpp"
#include "indexer/feature_data.hpp"

#include "base/assert.hpp"
#include "base/buffer_vector.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <map>
#include <sstream>
#include <unordered_map>

using namespace std;

namespace
{
class HighwayClasses
{
  map<uint32_t, ftypes::HighwayClass> m_map;

public:
  HighwayClasses()
  {
    auto const & c = classif();
    m_map[c.GetTypeByPath({"route", "ferry"})] = ftypes::HighwayClass::Transported;
    m_map[c.GetTypeByPath({"route", "shuttle_train"})] = ftypes::HighwayClass::Transported;
    m_map[c.GetTypeByPath({"railway", "rail"})] = ftypes::HighwayClass::Transported;

    m_map[c.GetTypeByPath({"highway", "motorway"})] = ftypes::HighwayClass::Trunk;
    m_map[c.GetTypeByPath({"highway", "motorway_link"})] = ftypes::HighwayClass::Trunk;
    m_map[c.GetTypeByPath({"highway", "trunk"})] = ftypes::HighwayClass::Trunk;
    m_map[c.GetTypeByPath({"highway", "trunk_link"})] = ftypes::HighwayClass::Trunk;

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
    m_map[c.GetTypeByPath({"man_made", "pier"})] = ftypes::HighwayClass::Service;

    m_map[c.GetTypeByPath({"highway", "pedestrian"})] = ftypes::HighwayClass::Pedestrian;
    m_map[c.GetTypeByPath({"highway", "footway"})] = ftypes::HighwayClass::Pedestrian;
    m_map[c.GetTypeByPath({"highway", "bridleway"})] = ftypes::HighwayClass::Pedestrian;
    m_map[c.GetTypeByPath({"highway", "steps"})] = ftypes::HighwayClass::Pedestrian;
    m_map[c.GetTypeByPath({"highway", "cycleway"})] = ftypes::HighwayClass::Pedestrian;
    m_map[c.GetTypeByPath({"highway", "path"})] = ftypes::HighwayClass::Pedestrian;
    m_map[c.GetTypeByPath({"highway", "construction"})] = ftypes::HighwayClass::Pedestrian;
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
  case ftypes::HighwayClass::Transported: return "Transported";
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

string DebugPrint(LocalityType const localityType)
{
  switch (localityType)
  {
  case LocalityType::None: return "None";
  case LocalityType::Country: return "Country";
  case LocalityType::State: return "State";
  case LocalityType::City: return "City";
  case LocalityType::Town: return "Town";
  case LocalityType::Village: return "Village";
  case LocalityType::Count: return "Count";
  }
  UNREACHABLE();
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

void BaseChecker::ForEachType(function<void(uint32_t)> && fn) const
{
  for_each(m_types.cbegin(), m_types.cend(), move(fn));
}

bool BaseChecker::operator()(feature::TypesHolder const & types) const
{
  for (uint32_t t : types)
  {
    if (IsMatched(t))
      return true;
  }

  return false;
}

bool BaseChecker::operator()(FeatureType & ft) const
{
  return this->operator()(feature::TypesHolder(ft));
}

bool BaseChecker::operator()(vector<uint32_t> const & types) const
{
  for (uint32_t t : types)
  {
    if (IsMatched(t))
      return true;
  }

  return false;
}

IsPeakChecker::IsPeakChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"natural", "peak"}));
}

IsATMChecker::IsATMChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"amenity", "atm"}));
}

IsSpeedCamChecker::IsSpeedCamChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"highway", "speed_camera"}));
}

IsPostBoxChecker::IsPostBoxChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"amenity", "post_box"}));
}

IsFuelStationChecker::IsFuelStationChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"amenity", "fuel"}));
}

IsRailwayStationChecker::IsRailwayStationChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"railway", "station"}));
  m_types.push_back(c.GetTypeByPath({"building", "train_station"}));
}

IsOutdoorChecker::IsOutdoorChecker()
{
  Classificator const & c = classif();
  char const * arr[][2] = {{"natural", "bare_rock"},
                           {"natural", "cave_entrance"},
                           {"natural", "cliff"},
                           {"natural", "peak"},
                           {"natural", "rock"},
                           {"natural", "volcano"},

                           {"natural", "geyser"},
                           {"natural", "hot_spring"},
                           {"natural", "lake"},
                           {"natural", "pond"},
                           {"natural", "salt_pond"},
                           {"natural", "spring"},
                           {"natural", "water"},
                           {"natural", "wetland"},
                           {"waterway", "waterfall"},

                           {"natural", "beach"},
                           {"natural", "cape"},
                           {"natural", "glacier"},
                           {"highway", "ford"}};
  for (auto const & p : arr)
    m_types.push_back(c.GetTypeByPath({p[0], p[1]}));
}

IsTransportChecker::IsTransportChecker()
{
  Classificator const & c = classif();
  char const * arr[][2] = {{"aeroway", "aerodrome"},
                           {"amenity", "bus_station"},
                           {"amenity", "car_sharing"},
                           {"amenity", "ferry_terminal"},
                           {"amenity", "taxi"},
                           {"building", "train_station"},
                           {"highway", "bus_stop"},
                           {"public_transport", "platform"},
                           {"railway", "halt"},
                           {"railway", "station"},
                           {"railway", "tram_stop"}};
  for (auto const & p : arr)
    m_types.push_back(c.GetTypeByPath({p[0], p[1]}));
}

IsParkingChecker::IsParkingChecker()
{
  Classificator const & c = classif();
  char const * arr[][2] = {{"amenity", "bicycle_parking"},
                           {"amenity", "motorcycle_parking"},
                           {"amenity", "parking"},
                           {"highway", "services"},
                           {"tourism", "caravan_site"}};
  for (auto const & p : arr)
    m_types.push_back(c.GetTypeByPath({p[0], p[1]}));
}

IsSubwayStationChecker::IsSubwayStationChecker() : BaseChecker(3 /* level */)
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"railway", "station", "subway"}));
}

IsAirportChecker::IsAirportChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"aeroway", "aerodrome"}));
}

IsSquareChecker::IsSquareChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"place", "square"}));
}

IsSuburbChecker::IsSuburbChecker()
{
  Classificator const & c = classif();
  auto const residentialType = c.GetTypeByPath({"landuse", "residential"});
  auto const neighbourhoodType = c.GetTypeByPath({"place", "neighbourhood"});
  auto const suburbType = c.GetTypeByPath({"place", "suburb"});
  m_types.push_back(residentialType);
  m_types.push_back(neighbourhoodType);
  m_types.push_back(suburbType);
  CHECK(m_types[static_cast<size_t>(SuburbType::Residential)] == residentialType, ());
  CHECK(m_types[static_cast<size_t>(SuburbType::Neighbourhood)] == neighbourhoodType, ());
  CHECK(m_types[static_cast<size_t>(SuburbType::Suburb)] == suburbType, ());
}

SuburbType IsSuburbChecker::GetType(uint32_t t) const
{
  ftype::TruncValue(t, 2);

  for (size_t i = 0; i < m_types.size(); ++i)
  {
    if (m_types[i] == t)
      return static_cast<SuburbType>(i);
  }

  return SuburbType::None;
}

SuburbType IsSuburbChecker::GetType(feature::TypesHolder const & types) const
{
  auto smallestType = SuburbType::Count;
  for (uint32_t t : types)
  {
    auto const type = GetType(t);
    if (type != SuburbType::None && type < smallestType)
      smallestType = type;
  }

  if (smallestType != SuburbType::Count)
    return smallestType;

  return SuburbType::None;
}

SuburbType IsSuburbChecker::GetType(FeatureType & f) const
{
  feature::TypesHolder types(f);
  return GetType(types);
}

IsWayChecker::IsWayChecker()
{
  // TODO (@y, @m, @vng): this list must be up-to-date with
  // data/categories.txt, so, it's worth it to generate or parse it
  // from that file.
  Classificator const & c = classif();
  char const * arr[][2] = {{"highway", "living_street"},
                           {"highway", "footway"},
                           {"highway", "cycleway"},
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

IsStreetOrSquareChecker::IsStreetOrSquareChecker()
{
  for (auto const t : IsWayChecker::Instance().GetTypes())
    m_types.push_back(t);
  for (auto const t : IsSquareChecker::Instance().GetTypes())
    m_types.push_back(t);
}

IsAddressObjectChecker::IsAddressObjectChecker() : BaseChecker(1 /* level */)
{
  auto const paths = {"building", "amenity", "shop", "tourism", "historic", "office", "craft"};

  Classificator const & c = classif();
  for (auto const & p : paths)
    m_types.push_back(c.GetTypeByPath({p}));
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

IsOneWayChecker::IsOneWayChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"hwtag", "oneway"}));
}

IsRoundAboutChecker::IsRoundAboutChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"junction", "roundabout"}));
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

IsBuildingChecker::IsBuildingChecker() : BaseChecker(1 /* level */)
{
  m_types.push_back(classif().GetTypeByPath({"building"}));
}

IsBuildingPartChecker::IsBuildingPartChecker() : BaseChecker(1 /* level */)
{
  m_types.push_back(classif().GetTypeByPath({"building:part"}));
}

IsIsolineChecker::IsIsolineChecker() : BaseChecker(1 /* level */)
{
  m_types.push_back(classif().GetTypeByPath({"isoline"}));
}

// static
set<string> const IsPoiChecker::kPoiTypes = {
  "amenity",
  "shop",
  "tourism",
  "leisure",
  "sport",
  "craft",
  "man_made",
  "emergency",
  "office",
  "historic",
  "railway",
  "highway",
  "aeroway"
};

IsPoiChecker::IsPoiChecker() : BaseChecker(1 /* level */)
{
  for (auto const & type : IsPoiChecker::kPoiTypes)
    m_types.push_back(classif().GetTypeByPath({type}));
}

AttractionsChecker::AttractionsChecker() : BaseChecker(2 /* level */)
{
  set<pair<string, string>> const primaryAttractionTypes = {
      {"amenity", "grave_yard"},
      {"amenity", "fountain"},
      {"amenity", "place_of_worship"},
      {"amenity", "theatre"},
      {"amenity", "townhall"},
      {"amenity", "university"},
      {"boundary", "national_park"},
      {"building", "train_station"},
      {"highway", "pedestrian"},
      {"historic", "archaeological_site"},
      {"historic", "boundary_stone"},
      {"historic", "castle"},
      {"historic", "fort"},
      {"historic", "memorial"},
      {"historic", "monument"},
      {"historic", "museum"},
      {"historic", "ruins"},
      {"historic", "ship"},
      {"historic", "tomb"},
      {"historic", "wayside_cross"},
      {"historic", "wayside_shrine"},
      {"landuse", "cemetery"},
      {"leisure", "garden"},
      {"leisure", "nature_reserve"},
      {"leisure", "park"},
      {"leisure", "water_park"},
      {"man_made", "lighthouse"},
      {"man_made", "tower"},
      {"natural", "beach"},
      {"natural", "cave_entrance"},
      {"natural", "geyser"},
      {"natural", "glacier"},
      {"natural", "hot_spring"},
      {"natural", "peak"},
      {"natural", "volcano"},
      {"place", "square"},
      {"tourism", "artwork"},
      {"tourism", "museum"},
      {"tourism", "gallery"},
      {"tourism", "zoo"},
      {"tourism", "theme_park"},
      {"waterway", "waterfall"},
  };

  set<pair<string, string>> const additionalAttractionTypes = {
      {"tourism", "viewpoint"},
      {"tourism", "attraction"},
  };

  for (auto const & t : primaryAttractionTypes)
  {
    auto const type = classif().GetTypeByPath({t.first, t.second});
    m_types.push_back(type);
    m_primaryTypes.push_back(type);
  }
  sort(m_primaryTypes.begin(), m_primaryTypes.end());

  for (auto const & t : additionalAttractionTypes)
  {
    auto const type = classif().GetTypeByPath({t.first, t.second});
    m_types.push_back(type);
    m_additionalTypes.push_back(type);
  }
  sort(m_additionalTypes.begin(), m_additionalTypes.end());
}

uint32_t AttractionsChecker::GetBestType(FeatureParams::Types const & types) const
{
  auto additionalType = ftype::GetEmptyValue();
  for (auto type : types)
  {
    type = PrepareToMatch(type, m_level);
    if (binary_search(m_primaryTypes.begin(), m_primaryTypes.end(), type))
      return type;

    if (binary_search(m_additionalTypes.begin(), m_additionalTypes.end(), type))
      additionalType = type;
  }
  return additionalType;
}

IsPlaceChecker::IsPlaceChecker() : BaseChecker(1 /* level */)
{
  m_types.push_back(classif().GetTypeByPath({"place"}));
}

IsBridgeChecker::IsBridgeChecker() : BaseChecker(3 /* level */) {}

bool IsBridgeChecker::IsMatched(uint32_t type) const
{
  return IsTypeConformed(type, {"highway", "*", "bridge"});
}

IsTunnelChecker::IsTunnelChecker() : BaseChecker(3 /* level */) {}

bool IsTunnelChecker::IsMatched(uint32_t type) const
{
  return IsTypeConformed(type, {"highway", "*", "tunnel"});
}

IsHotelChecker::IsHotelChecker()
{
  Classificator const & c = classif();
  for (size_t i = 0; i < static_cast<size_t>(Type::Count); ++i)
  {
    auto const hotelType = static_cast<Type>(i);
    auto const * const tag = GetHotelTypeTag(hotelType);
    auto const type = c.GetTypeByPath({"tourism", tag});

    m_types.push_back(type);

    m_sortedTypes[i].first = type;
    m_sortedTypes[i].second = hotelType;
  }

  sort(m_sortedTypes.begin(), m_sortedTypes.end());
}

unsigned IsHotelChecker::GetHotelTypesMask(FeatureType & ft) const
{
  feature::TypesHolder types(ft);
  buffer_vector<uint32_t, feature::kMaxTypesCount> sortedTypes(types.begin(), types.end());
  sort(sortedTypes.begin(), sortedTypes.end());

  unsigned mask = 0;
  size_t i = 0;
  size_t j = 0;
  while (i < sortedTypes.size() && j < m_sortedTypes.size())
  {
    if (sortedTypes[i] < m_sortedTypes[j].first)
    {
      ++i;
    }
    else if (sortedTypes[i] > m_sortedTypes[j].first)
    {
      ++j;
    }
    else
    {
      mask |= 1U << static_cast<unsigned>(m_sortedTypes[j].second);
      ++i;
      ++j;
    }
  }

  return mask;
}

IsIslandChecker::IsIslandChecker()
{
  vector<pair<string, string>> const types = {
    {"place", "island"},
    {"place", "islet"},
  };

  Classificator const & c = classif();
  for (auto const & t : types)
    m_types.push_back(c.GetTypeByPath({t.first, t.second}));
}

IsLandChecker::IsLandChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"natural", "land"}));
}

uint32_t IsLandChecker::GetLandType() const
{
  CHECK_EQUAL(m_types.size(), 1, ());
  return m_types[0];
}

IsCoastlineChecker::IsCoastlineChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"natural", "coastline"}));
}

uint32_t IsCoastlineChecker::GetCoastlineType() const
{
  CHECK_EQUAL(m_types.size(), 1, ());
  return m_types[0];
}

optional<IsHotelChecker::Type> IsHotelChecker::GetHotelType(FeatureType & ft) const
{
  feature::TypesHolder types(ft);
  buffer_vector<uint32_t, feature::kMaxTypesCount> sortedTypes(types.begin(), types.end());
  sort(sortedTypes.begin(), sortedTypes.end());

  size_t i = 0;
  size_t j = 0;
  while (i < sortedTypes.size() && j < m_sortedTypes.size())
  {
    if (sortedTypes[i] < m_sortedTypes[j].first)
      ++i;
    else if (sortedTypes[i] > m_sortedTypes[j].first)
      ++j;
    else
      return m_sortedTypes[j].second;
  }
  return {};
}

// static
char const * IsHotelChecker::GetHotelTypeTag(Type type)
{
  switch (type)
  {
  case Type::Hotel: return "hotel";
  case Type::Apartment: return "apartment";
  case Type::CampSite: return "camp_site";
  case Type::Chalet: return "chalet";
  case Type::GuestHouse: return "guest_house";
  case Type::Hostel: return "hostel";
  case Type::Motel: return "motel";
  case Type::Resort: return "resort";
  case Type::Count: CHECK(false, ("Can't get hotel type tag")); return "";
  }
  UNREACHABLE();
}

IsBookingHotelChecker::IsBookingHotelChecker()
{
  m_types.push_back(classif().GetTypeByPath({"sponsored", "booking"}));
}

IsWifiChecker::IsWifiChecker()
{
  m_types.push_back(classif().GetTypeByPath({"internet_access", "wlan"}));
}

IsEatChecker::IsEatChecker()
{
  Classificator const & c = classif();
  map<Type, vector<string>> const descriptions = {{Type::Cafe,       {"amenity", "cafe"}},
                                                  {Type::Bakery,     {"shop", "bakery"}},
                                                  {Type::FastFood,   {"amenity", "fast_food"}},
                                                  {Type::Restaurant, {"amenity", "restaurant"}},
                                                  {Type::Bar,        {"amenity", "bar"}},
                                                  {Type::Pub,        {"amenity", "pub"}},
                                                  {Type::Biergarten, {"amenity", "biergarten"}}};

  for (auto const & desc : descriptions)
  {
    auto const type = c.GetTypeByPath(desc.second);
    m_types.push_back(type);
    m_sortedTypes[static_cast<size_t>(desc.first)] = {type, desc.first};
  }
}

IsEatChecker::Type IsEatChecker::GetType(uint32_t t) const
{
  for (auto type : m_sortedTypes)
  {
    if (type.first == t)
      return type.second;
  }

  return IsEatChecker::Type::Count;
}

IsCuisineChecker::IsCuisineChecker() : BaseChecker(1 /* level */)
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"cuisine"}));
}

IsRecyclingTypeChecker::IsRecyclingTypeChecker() : BaseChecker(1 /* level */)
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"recycling"}));
}

IsCityChecker::IsCityChecker()
{
  m_types.push_back(classif().GetTypeByPath({"place", "city"}));
}

IsCapitalChecker::IsCapitalChecker() : BaseChecker(3 /* level */)
{
  m_types.push_back(classif().GetTypeByPath({"place", "city", "capital"}));
}

IsPublicTransportStopChecker::IsPublicTransportStopChecker()
{
  m_types.push_back(classif().GetTypeByPath({"highway", "bus_stop"}));
  m_types.push_back(classif().GetTypeByPath({"railway", "tram_stop"}));
}

IsMotorwayJunctionChecker::IsMotorwayJunctionChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"highway", "motorway_junction"}));
}

IsFerryChecker::IsFerryChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"route", "ferry"}));
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

LocalityType IsLocalityChecker::GetType(uint32_t t) const
{
  ftype::TruncValue(t, 2);

  auto j = static_cast<size_t>(LocalityType::Country);
  for (; j < static_cast<size_t>(LocalityType::Count); ++j)
    if (t == m_types[j])
      return static_cast<LocalityType>(j);

  for (; j < m_types.size(); ++j)
    if (t == m_types[j])
      return LocalityType::Village;

  return LocalityType::None;
}

LocalityType IsLocalityChecker::GetType(feature::TypesHolder const & types) const
{
  for (uint32_t t : types)
  {
    LocalityType const type = GetType(t);
    if (type != LocalityType::None)
      return type;
  }
  return LocalityType::None;
}

LocalityType IsLocalityChecker::GetType(FeatureType & f) const
{
  feature::TypesHolder types(f);
  return GetType(types);
}

IsCountryChecker::IsCountryChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"place", "country"}));
}

IsStateChecker::IsStateChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"place", "state"}));
}

IsCityTownOrVillageChecker::IsCityTownOrVillageChecker()
{
  vector<pair<string, string>> const types = {
    {"place", "city"},
    {"place", "town"},
    {"place", "village"},
    {"place", "hamlet"}
  };

  Classificator const & c = classif();
  for (auto const & t : types)
    m_types.push_back(c.GetTypeByPath({t.first, t.second}));
}

uint64_t GetPopulation(FeatureType & ft)
{
  uint64_t population = ft.GetPopulation();

  if (population < 10)
  {
    switch (IsLocalityChecker::Instance().GetType(ft))
    {
    case LocalityType::Country: population = 500000; break;
    case LocalityType::State: population = 100000; break;
    case LocalityType::City: population = 50000; break;
    case LocalityType::Town: population = 10000; break;
    case LocalityType::Village: population = 100; break;
    default: population = 0;
    }
  }

  return population;
}

double GetRadiusByPopulation(uint64_t p)
{
  return pow(static_cast<double>(p), 1 / 3.6) * 550.0;
}

// Look to: https://confluence.mail.ru/pages/viewpage.action?pageId=287950469
// for details about factors.
// Shortly, we assume: radius = (population ^ (1 / base)) * mult
// We knew area info about some cities|towns|villages and did grid search.
// Interval for base: [0.1, 100].
// Interval for mult: [10, 1000].
double GetRadiusByPopulationForRouting(uint64_t p, LocalityType localityType)
{
  switch (localityType)
  {
  case LocalityType::City: return pow(static_cast<double>(p), 1.0 / 2.5) * 34.0;
  case LocalityType::Town: return pow(static_cast<double>(p), 1.0 / 6.8) * 354.0;
  case LocalityType::Village: return pow(static_cast<double>(p), 1.0 / 15.1) * 610.0;
  default: UNREACHABLE();
  }
}

uint64_t GetPopulationByRadius(double r)
{
  return base::SignedRound(pow(r / 550.0, 3.6));
}

bool IsTypeConformed(uint32_t type, base::StringIL const & path)
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
