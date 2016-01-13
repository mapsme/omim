#include "categories_holder.hpp"
#include "intermediate_result.hpp"
#include "geometry_utils.hpp"

#include "storage/country_info_getter.hpp"

#include "indexer/ftypes_matcher.hpp"
#include "indexer/classificator.hpp"
#include "indexer/feature.hpp"
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


void ProcessMetadata(FeatureType const & ft, Result::Metadata & meta)
{
  ft.ParseMetadata();
  feature::Metadata const & src = ft.GetMetadata();

  meta.m_cuisine = src.Get(feature::Metadata::FMD_CUISINE);

  string const openHours = src.Get(feature::Metadata::FMD_OPEN_HOURS);
  if (!openHours.empty())
  {
    osmoh::OpeningHours oh(openHours);

    // TODO(mgsergio): Switch to three-stated model instead of two-staed
    // I.e. set unknown if we can't parse or can't answer whether it's open.
    if (oh.IsValid())
     meta.m_isClosed = oh.IsClosed(time(nullptr));
    else
      meta.m_isClosed = false;
  }

  meta.m_stars = 0;
  (void) strings::to_int(src.Get(feature::Metadata::FMD_STARS), meta.m_stars);
  meta.m_stars = my::clamp(meta.m_stars, 0, 5);
}

namespace impl
{
PreResult1::PreResult1(FeatureID const & fID, uint8_t rank, double priority, int8_t viewportID)
  : m_id(fID), m_priority(priority), m_rank(rank), m_viewportID(viewportID)
{
  ASSERT(m_id.IsValid(), ());
}

PreResult1::PreResult1(double priority) : m_priority(priority) {}

// static
bool PreResult1::LessRank(PreResult1 const & r1, PreResult1 const & r2)
{
  if (r1.m_rank != r2.m_rank)
    return r1.m_rank > r2.m_rank;
  return r1.m_priority < r2.m_priority;
}

// static
bool PreResult1::LessPriority(PreResult1 const & r1, PreResult1 const & r2)
{
  if (r1.m_priority != r2.m_priority)
    return r1.m_priority < r2.m_priority;
  return r1.m_rank > r2.m_rank;
}

// static
bool PreResult1::LessPointsForViewport(PreResult1 const & r1, PreResult1 const & r2)
{
  return r1.m_id < r2.m_id;
}

PreResult2::PreResult2(FeatureType const & f, PreResult1 const * p, m2::PointD const & pivot,
                       string const & displayName, string const & fileName)
  : m_id(f.GetID()),
    m_types(f),
    m_str(displayName),
    m_resultType(RESULT_FEATURE),
    m_geomType(f.GetFeatureType())
{
  ASSERT(m_id.IsValid(), ());
  ASSERT(!m_types.Empty(), ());

  m_types.SortBySpec();

  m_rank = p ? p->GetRank() : 0;

  m2::PointD fCenter;
  fCenter = f.GetLimitRect(FeatureType::WORST_GEOMETRY).Center();

  m_region.SetParams(fileName, fCenter);
  m_distance = PointDistance(GetCenter(), pivot);

  ProcessMetadata(f, m_metadata);
}

PreResult2::PreResult2(double lat, double lon)
  : m_str("(" + MeasurementUtils::FormatLatLon(lat, lon) + ")"),
    m_resultType(RESULT_LATLON)
{
  m_region.SetParams(string(), MercatorBounds::FromLatLon(lat, lon));
}

PreResult2::PreResult2(m2::PointD const & pt, string const & str, uint32_t type)
  : m_str(str), m_resultType(RESULT_BUILDING)
{
  m_region.SetParams(string(), pt);

  m_types.Assign(type);
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

Result PreResult2::GenerateFinalResult(storage::CountryInfoGetter const & infoGetter,
                                       CategoriesHolder const * pCat,
                                       set<uint32_t> const * pTypes, int8_t locale) const
{
  uint32_t const type = GetBestType(pTypes);
  string const regionName = GetRegionName(infoGetter, type);

  switch (m_resultType)
  {
  case RESULT_FEATURE:
    return Result(m_id, GetCenter(), m_str, regionName, ReadableFeatureType(pCat, type, locale)
              #ifdef DEBUG
                  + ' ' + strings::to_string(int(m_rank))
              #endif
                  , type, m_metadata);

  case RESULT_BUILDING:
    return Result(GetCenter(), m_str, regionName, ReadableFeatureType(pCat, type, locale));

  default:
    ASSERT_EQUAL(m_resultType, RESULT_LATLON, ());
    return Result(GetCenter(), m_str, regionName, string());
  }
}

Result PreResult2::GeneratePointResult(storage::CountryInfoGetter const & infoGetter,
                                       CategoriesHolder const * pCat,
                                       set<uint32_t> const * pTypes, int8_t locale) const
{
  uint32_t const type = GetBestType(pTypes);
  return Result(m_id, GetCenter(), m_str, GetRegionName(infoGetter, type),
                ReadableFeatureType(pCat, type, locale));
}

// static
bool PreResult2::LessRank(PreResult2 const & r1, PreResult2 const & r2)
{
  if (r1.m_rank != r2.m_rank)
    return r1.m_rank > r2.m_rank;
  return r1.m_distance < r2.m_distance;
}

// static
bool PreResult2::LessDistance(PreResult2 const & r1, PreResult2 const & r2)
{
  if (r1.m_distance != r2.m_distance)
    return r1.m_distance < r2.m_distance;
  return r1.m_rank > r2.m_rank;
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
        "; Rank: " << int(m_rank) <<
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

string PreResult2::ReadableFeatureType(CategoriesHolder const * pCat,
                                       uint32_t type, int8_t locale) const
{
  ASSERT_NOT_EQUAL(type, 0, ());
  if (pCat)
  {
    string name;
    if (pCat->GetNameByType(type, locale, name))
      return name;
  }

  return classif().GetReadableObjectName(type);
}

void PreResult2::RegionInfo::GetRegion(storage::CountryInfoGetter const & infoGetter,
                                       storage::CountryInfo & info) const
{
  if (!m_file.empty())
    infoGetter.GetRegionInfo(m_file, info);
  else
    infoGetter.GetRegionInfo(m_point, info);
}
}  // namespace search::impl
}  // namespace search
