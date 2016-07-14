#include "intermediate_result.hpp"
#include "geometry_utils.hpp"
#include "reverse_geocoder.hpp"

#include "storage/country_info_getter.hpp"

#include "indexer/categories_holder.hpp"
#include "indexer/classificator.hpp"
#include "indexer/feature.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/scales.hpp"

#include "geometry/angles.hpp"

#include "platform/measurement_utils.hpp"

#include "base/string_utils.hpp"
#include "base/logging.hpp"

#include "3party/opening_hours/opening_hours.hpp"

namespace search
{

/// All constants in meters.
double const DIST_EQUAL_RESULTS = 100.0;
double const DIST_SAME_STREET = 5000.0;


void ProcessMetadata(FeatureType const & ft, search::Metadata & meta)
{
  if (meta.m_isInitialized)
    return;

  bool const isSponsoredHotel = ftypes::IsBookingChecker::Instance()(ft);
  feature::Metadata const & src = ft.GetMetadata();

  meta.m_cuisine = src.Get(feature::Metadata::FMD_CUISINE);
  meta.m_stars = src.Get(feature::Metadata::FMD_STARS);
  meta.m_operator = src.Get(feature::Metadata::FMD_OPERATOR);
  meta.m_schedule = src.Get(feature::Metadata::FMD_OPEN_HOURS);
  meta.m_elevation = src.Get(feature::Metadata::FMD_ELE);
  meta.m_isSponsoredHotel = isSponsoredHotel;
  if (isSponsoredHotel)
  {
    meta.m_hotelRating = src.Get(feature::Metadata::FMD_RATING);
    meta.m_hotelPriceRate = src.Get(feature::Metadata::FMD_PRICE_RATE);
  }

  meta.m_isInitialized = true;
}

PreResult1::PreResult1(FeatureID const & fID, PreRankingInfo const & info) : m_id(fID), m_info(info)
{
  ASSERT(m_id.IsValid(), ());
}

// static
bool PreResult1::LessRank(PreResult1 const & r1, PreResult1 const & r2)
{
  if (r1.m_info.m_rank != r2.m_info.m_rank)
    return r1.m_info.m_rank > r2.m_info.m_rank;
  return r1.m_info.m_distanceToPivot < r2.m_info.m_distanceToPivot;
}

// static
bool PreResult1::LessDistance(PreResult1 const & r1, PreResult1 const & r2)
{
  if (r1.m_info.m_distanceToPivot != r2.m_info.m_distanceToPivot)
    return r1.m_info.m_distanceToPivot < r2.m_info.m_distanceToPivot;
  return r1.m_info.m_rank > r2.m_info.m_rank;
}

PreResult2::PreResult2(FeatureType const & f, PreResult1 const * p, m2::PointD const & center,
                       m2::PointD const & pivot, string const & displayName,
                       string const & fileName)
  : m_id(f.GetID())
  , m_types(f)
  , m_str(displayName)
  , m_resultType(ftypes::IsBuildingChecker::Instance()(m_types) ? RESULT_BUILDING : RESULT_FEATURE)
  , m_geomType(f.GetFeatureType())
{
  ASSERT(m_id.IsValid(), ());
  ASSERT(!m_types.Empty(), ());

  m_types.SortBySpec();

  m_region.SetParams(fileName, center);
  m_distance = PointDistance(center, pivot);

  ProcessMetadata(f, m_metadata);
}

PreResult2::PreResult2(double lat, double lon)
  : m_str("(" + measurement_utils::FormatLatLon(lat, lon) + ")"), m_resultType(RESULT_LATLON)
{
  m_region.SetParams(string(), MercatorBounds::FromLatLon(lat, lon));
}

namespace
{
  class SkipRegionInfo
  {
    static size_t const m_count = 2;
    uint32_t m_types[m_count];

  public:
    SkipRegionInfo()
    {
      char const * arr[][2] = {
        { "place", "continent" },
        { "place", "country" }
      };
      static_assert(m_count == ARRAY_SIZE(arr), "");

      Classificator const & c = classif();
      for (size_t i = 0; i < m_count; ++i)
        m_types[i] = c.GetTypeByPath(vector<string>(arr[i], arr[i] + 2));
    }

    bool IsSkip(uint32_t type) const
    {
      for (uint32_t t : m_types)
      {
        if (t == type)
          return true;
      }
      return false;
    }
  };
}

string PreResult2::GetRegionName(storage::CountryInfoGetter const & infoGetter,
                                 uint32_t fType) const
{
  static SkipRegionInfo const checker;
  if (checker.IsSkip(fType))
    return string();

  storage::CountryInfo info;
  m_region.GetRegion(infoGetter, info);
  return info.m_name;
}

namespace
{
// TODO: Format street and house number according to local country's rules.
string FormatStreetAndHouse(ReverseGeocoder::Address const & addr)
{
  ASSERT_GREATER_OR_EQUAL(addr.GetDistance(), 0, ());
  return addr.GetStreetName() + ", " + addr.GetHouseNumber();
}

// TODO: Share common formatting code for search results and place page.
string FormatFullAddress(ReverseGeocoder::Address const & addr, string const & region)
{
  // TODO: Print "near" for not exact addresses.
  if (addr.GetDistance() != 0)
    return region;

  return FormatStreetAndHouse(addr) + (region.empty() ? "" : ", ") + region;
}
}  // namespace

Result PreResult2::GenerateFinalResult(storage::CountryInfoGetter const & infoGetter,
                                       CategoriesHolder const * pCat,
                                       set<uint32_t> const * pTypes, int8_t locale,
                                       ReverseGeocoder const * coder) const
{
  string name = m_str;
  if (coder && name.empty())
  {
    // Insert exact address (street and house number) instead of empty result name.
    ReverseGeocoder::Address addr;
    coder->GetNearbyAddress(GetCenter(), addr);
    if (addr.GetDistance() == 0)
      name = FormatStreetAndHouse(addr);
  }

  uint32_t const type = GetBestType(pTypes);

  // Format full address only for suitable results.
  string address;
  if (coder)
  {
    address = GetRegionName(infoGetter, type);
    if (ftypes::IsAddressObjectChecker::Instance()(m_types))
    {
      ReverseGeocoder::Address addr;
      coder->GetNearbyAddress(GetCenter(), addr);
      address = FormatFullAddress(addr, address);
    }
  }

  switch (m_resultType)
  {
  case RESULT_FEATURE:
  case RESULT_BUILDING:
    return Result(m_id, GetCenter(), name, address, pCat->GetReadableFeatureType(type, locale),
                  type, m_metadata);
  default:
    ASSERT_EQUAL(m_resultType, RESULT_LATLON, ());
    return Result(GetCenter(), name, address);
  }
}

bool PreResult2::StrictEqualF::operator() (PreResult2 const & r) const
{
  if (m_r.m_resultType == r.m_resultType && m_r.m_resultType == RESULT_FEATURE)
  {
    if (m_r.IsEqualCommon(r))
      return (PointDistance(m_r.GetCenter(), r.GetCenter()) < DIST_EQUAL_RESULTS);
  }

  return false;
}

bool PreResult2::LessLinearTypesF::operator() (PreResult2 const & r1, PreResult2 const & r2) const
{
  if (r1.m_geomType != r2.m_geomType)
    return (r1.m_geomType < r2.m_geomType);

  if (r1.m_str != r2.m_str)
    return (r1.m_str < r2.m_str);

  uint32_t const t1 = r1.GetBestType();
  uint32_t const t2 = r2.GetBestType();
  if (t1 != t2)
    return (t1 < t2);

  // Should stay the best feature, after unique, so add this criteria:
  return (r1.m_distance < r2.m_distance);
}

bool PreResult2::EqualLinearTypesF::operator() (PreResult2 const & r1, PreResult2 const & r2) const
{
  // Note! Do compare for distance when filtering linear objects.
  // Otherwise we will skip the results for different parts of the map.
  return (r1.m_geomType == feature::GEOM_LINE &&
          r1.IsEqualCommon(r2) &&
          PointDistance(r1.GetCenter(), r2.GetCenter()) < DIST_SAME_STREET);
}

bool PreResult2::IsEqualCommon(PreResult2 const & r) const
{
  return (m_geomType == r.m_geomType &&
          GetBestType() == r.GetBestType() &&
          m_str == r.m_str);
}

bool PreResult2::IsStreet() const
{
  return (m_geomType == feature::GEOM_LINE &&
          ftypes::IsStreetChecker::Instance()(m_types));
}

string PreResult2::DebugPrint() const
{
  stringstream ss;
  ss << "{ IntermediateResult: " <<
        "Name: " << m_str <<
        "; Type: " << GetBestType() <<
        "; Rank: " << static_cast<int>(m_info.m_rank) <<
        "; Distance: " << m_distance << " }";
  return ss.str();
}

uint32_t PreResult2::GetBestType(set<uint32_t> const * pPrefferedTypes) const
{
  if (pPrefferedTypes)
  {
    for (uint32_t type : m_types)
    {
      if (pPrefferedTypes->count(type) > 0)
        return type;
    }
  }

  // Do type truncate (2-level is enough for search results) only for
  // non-preffered types (types from categories leave original).
  uint32_t type = m_types.GetBestType();
  ftype::TruncValue(type, 2);
  return type;
}

void PreResult2::RegionInfo::GetRegion(storage::CountryInfoGetter const & infoGetter,
                                       storage::CountryInfo & info) const
{
  if (!m_file.empty())
    infoGetter.GetRegionInfo(m_file, info);
  else
    infoGetter.GetRegionInfo(m_point, info);
}
}  // namespace search
