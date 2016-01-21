#include "storage/country.hpp"
#include "storage/country_info_getter.hpp"
#include "storage/country_polygon.hpp"

#include "indexer/geometry_serialization.hpp"

#include "geometry/latlon.hpp"
#include "geometry/mercator.hpp"
#include "geometry/region2d.hpp"

#include "coding/read_write_utils.hpp"

#include "base/string_utils.hpp"

#include "3party/Alohalytics/src/alohalytics.h"

#include "std/bind.hpp"
#include "std/function.hpp"
#include "std/limits.hpp"

namespace storage
{
namespace
{
size_t const kInvalidId = numeric_limits<size_t>::max();

struct DoFreeCacheMemory
{
  void operator()(vector<m2::RegionD> & v) const { vector<m2::RegionD>().swap(v); }
};

class DoCalcUSA
{
public:
  DoCalcUSA(m2::RectD * rects) : m_rects(rects) {}

  void operator()(CountryDef const & c)
  {
    if (c.m_name == "USA_Alaska")
      m_rects[1] = c.m_rect;
    else if (c.m_name == "USA_Hawaii")
      m_rects[2] = c.m_rect;
    else
      m_rects[0].Add(c.m_rect);
  }

private:
  m2::RectD * const m_rects;
};
}  // namespace

CountryInfoGetter::CountryInfoGetter(ModelReaderPtr polyR, ModelReaderPtr countryR)
  : m_reader(polyR), m_cache(3), m_isSingleMwm(true)
{
  ReaderSource<ModelReaderPtr> src(m_reader.GetReader(PACKED_POLYGONS_INFO_TAG));
  rw::Read(src, m_countries);

  size_t const countrySz = m_countries.size();
  m_countryIndex.reserve(countrySz);
  for (size_t i = 0; i < countrySz; ++i)
    m_countryIndex[m_countries[i].m_name] = i;

  string buffer;
  countryR.ReadAsString(buffer);
  LoadCountryFile2CountryInfo(buffer, m_id2info, m_isSingleMwm);
}

TCountryId CountryInfoGetter::GetRegionCountryId(m2::PointD const & pt) const
{
  IdType const id = FindFirstCountry(pt);
  return id != kInvalidId ? m_countries[id].m_name : TCountryId();
}

void CountryInfoGetter::GetRegionsCountryId(m2::PointD const & pt, TCountriesVec & closestCoutryIds)
{
  // @TODO(bykoianko) Now this method fills |closestCoutryIds| with only a country id of mwm
  // which covers |pt|. This method should fill |closestCoutryIds| with several mwms
  // which cover |pt| and close to |pt|.
  closestCoutryIds.clear();
  TCountryId countryId = GetRegionCountryId(pt);
  if (!countryId.empty())
    closestCoutryIds.emplace_back(countryId);
}

void CountryInfoGetter::GetRegionInfo(m2::PointD const & pt, CountryInfo & info) const
{
  IdType const id = FindFirstCountry(pt);
  if (id != kInvalidId)
    GetRegionInfo(m_countries[id].m_name, info);
}

void CountryInfoGetter::GetRegionInfo(string const & id, CountryInfo & info) const
{
  auto const it = m_id2info.find(id);
  if (it == m_id2info.end())
    return;

  info = it->second;
  if (info.m_name.empty())
    info.m_name = id;
}

void CountryInfoGetter::CalcUSALimitRect(m2::RectD rects[3]) const
{
  ForEachCountry("USA_", DoCalcUSA(rects));
}

m2::RectD CountryInfoGetter::CalcLimitRect(string const & prefix) const
{
  m2::RectD rect;
  ForEachCountry(prefix, [&rect](CountryDef const & c)
  {
    rect.Add(c.m_rect);
  });
  return rect;
}

m2::RectD CountryInfoGetter::CalcLimitRectForLeaf(TCountryId leafCountryId) const
{
  auto const index = this->m_countryIndex.find(leafCountryId);
  ASSERT(index != this->m_countryIndex.end(), ());
  ASSERT_LESS(index->second, this->m_countries.size(), ());
  return m_countries[index->second].m_rect;
}

void CountryInfoGetter::GetMatchedRegions(string const & enNamePrefix, IdSet & regions) const
{
  for (size_t i = 0; i < m_countries.size(); ++i)
  {
    // Match english name with region file name (they are equal in almost all cases).
    // @todo Do it smarter in future.
    string s = m_countries[i].m_name;
    strings::AsciiToLower(s);
    if (strings::StartsWith(s, enNamePrefix.c_str()))
      regions.push_back(i);
  }
}

bool CountryInfoGetter::IsBelongToRegions(m2::PointD const & pt, IdSet const & regions) const
{
  for (auto const & id : regions)
  {
    if (m_countries[id].m_rect.IsPointInside(pt) && IsBelongToRegion(id, pt))
      return true;
  }
  return false;
}

bool CountryInfoGetter::IsBelongToRegions(string const & fileName, IdSet const & regions) const
{
  for (auto const & id : regions)
  {
    if (m_countries[id].m_name == fileName)
      return true;
  }
  return false;
}

void CountryInfoGetter::ClearCaches() const
{
  lock_guard<mutex> lock(m_cacheMutex);

  m_cache.ForEachValue(DoFreeCacheMemory());
  m_cache.Reset();
}

bool CountryInfoGetter::IsBelongToRegion(size_t id, m2::PointD const & pt) const
{
  lock_guard<mutex> lock(m_cacheMutex);

  bool isFound = false;
  vector<m2::RegionD> & rgns = m_cache.Find(static_cast<uint32_t>(id), isFound);

  if (!isFound)
  {
    rgns.clear();
    // Load regions from file.
    ReaderSource<ModelReaderPtr> src(m_reader.GetReader(strings::to_string(id)));

    uint32_t const count = ReadVarUint<uint32_t>(src);
    for (size_t i = 0; i < count; ++i)
    {
      vector<m2::PointD> points;
      serial::LoadOuterPath(src, serial::CodingParams(), points);
      rgns.emplace_back(move(points));
    }
  }

  for (auto const & rgn : rgns)
  {
    if (rgn.Contains(pt))
      return true;
  }
  return false;
}

CountryInfoGetter::IdType CountryInfoGetter::FindFirstCountry(m2::PointD const & pt) const
{
  for (size_t id = 0; id < m_countries.size(); ++id)
  {
    if (m_countries[id].m_rect.IsPointInside(pt) && IsBelongToRegion(id, pt))
      return id;
  }

  ms::LatLon const latLon = MercatorBounds::ToLatLon(pt);
  alohalytics::LogEvent(m_isSingleMwm ? "Small mwm case. CountryInfoGetter could not find any mwm by point."
                                      : "Big mwm case. CountryInfoGetter could not find any mwm by point.",
                        alohalytics::Location::FromLatLon(latLon.lat, latLon.lon));
  return kInvalidId;
}

template <typename ToDo>
void CountryInfoGetter::ForEachCountry(string const & prefix, ToDo && toDo) const
{
  for (auto const & country : m_countries)
  {
    if (strings::StartsWith(country.m_name, prefix.c_str()))
      toDo(country);
  }
}
}  // namespace storage
