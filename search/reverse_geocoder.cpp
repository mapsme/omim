#include "reverse_geocoder.hpp"

#include "search/v2/mwm_context.hpp"

#include "indexer/feature.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/index.hpp"
#include "indexer/scales.hpp"
#include "indexer/search_string_utils.hpp"

#include "base/stl_helpers.hpp"

#include "std/limits.hpp"

namespace search
{
namespace
{
size_t constexpr kSimilarityThresholdPercent = 10;
int constexpr kQueryScale = scales::GetUpperScale();
/// Max number of tries (nearest houses with housenumber) to check when getting point address.
size_t constexpr kMaxNumTriesToApproxAddress = 10;
} // namespace

ReverseGeocoder::ReverseGeocoder(Index const & index) : m_index(index) {}

void ReverseGeocoder::GetNearbyStreets(MwmSet::MwmId const & id, m2::PointD const & center,
                                       vector<Street> & streets, double const lookupRadiusM) const
{
  m2::RectD const rect = GetLookupRect(center, lookupRadiusM);

  auto const addStreet = [&](FeatureType & ft)
  {
    if (ft.GetFeatureType() != feature::GEOM_LINE ||
        !ftypes::IsStreetChecker::Instance()(ft))
    {
      return;
    }

    string name;
    if (!ft.GetName(StringUtf8Multilang::kDefaultCode, name))
      return;

    ASSERT(!name.empty(), ());

    streets.push_back({ft.GetID(), feature::GetMinDistanceMeters(ft, center), name});
  };

  MwmSet::MwmHandle mwmHandle = m_index.GetMwmHandleById(id);
  if (mwmHandle.IsAlive())
  {
    search::v2::MwmContext(move(mwmHandle)).ForEachFeature(rect, addStreet);
    sort(streets.begin(), streets.end(), my::CompareBy(&Street::m_distanceMeters));
  }
}

void ReverseGeocoder::GetNearbyStreets(FeatureType & ft, vector<Street> & streets,
                                       double const lookupRadiusM) const
{
  ASSERT(ft.GetID().IsValid(), ());
  GetNearbyStreets(ft.GetID().m_mwmId, feature::GetCenter(ft), streets, lookupRadiusM);
}

// static
size_t ReverseGeocoder::GetMatchedStreetIndex(string const & keyName,
                                              vector<Street> const & streets)
{
  strings::UniString const expected = strings::MakeUniString(keyName);

  // Find the exact match or the best match in kSimilarityTresholdPercent limit.
  size_t const count = streets.size();
  size_t result = count;
  size_t minPercent = kSimilarityThresholdPercent + 1;

  for (size_t i = 0; i < count; ++i)
  {
    string key;
    search::GetStreetNameAsKey(streets[i].m_name, key);
    strings::UniString const actual = strings::MakeUniString(key);

    size_t const editDistance =
        strings::EditDistance(expected.begin(), expected.end(), actual.begin(), actual.end());

    if (editDistance == 0)
      return i;

    if (actual.empty())
      continue;

    size_t const percent = editDistance * 100 / actual.size();
    if (percent < minPercent)
    {
      result = i;
      minPercent = percent;
    }
  }

  return result;
}

pair<vector<ReverseGeocoder::Street>, uint32_t>
ReverseGeocoder::GetNearbyFeatureStreets(FeatureType & ft) const
{
  pair<vector<ReverseGeocoder::Street>, uint32_t> result;

  GetNearbyStreets(ft, result.first);

  HouseTable table(m_index);
  if (!table.Get(ft.GetID(), result.second))
    result.second = numeric_limits<uint32_t>::max();

  return result;
}

void ReverseGeocoder::GetNearbyAddress(m2::PointD const & center, Address & addr) const
{
  vector<Building> buildings;
  GetNearbyBuildings(center, buildings);

  HouseTable table(m_index);
  size_t triesCount = 0;

  for (auto const & b : buildings)
  {
    // It's quite enough to analize nearest kMaxNumTriesToApproxAddress houses for the exact nearby address.
    // When we can't guarantee suitable address for the point with distant houses.
    if (GetNearbyAddress(table, b, addr) || (++triesCount == kMaxNumTriesToApproxAddress))
      break;
  }
}

void ReverseGeocoder::GetNearbyAddress(FeatureType & ft, Address & addr) const
{
  HouseTable table(m_index);
  (void)GetNearbyAddress(table, FromFeature(ft, 0.0 /* distMeters */), addr);
}

bool ReverseGeocoder::GetNearbyAddress(HouseTable & table, Building const & bld,
                                       Address & addr) const
{
  string street;
  if (osm::Editor::Instance().GetEditedFeatureStreet(bld.m_id, street))
  {
    addr.m_building = bld;
    addr.m_street.m_name = street;
    return true;
  }

  uint32_t ind;
  if (!table.Get(bld.m_id, ind))
    return false;

  vector<Street> streets;
  GetNearbyStreets(bld.m_id.m_mwmId, bld.m_center, streets);
  if (ind < streets.size())
  {
    addr.m_building = bld;
    addr.m_street = streets[ind];
    return true;
  }

  uint32_t const bit = static_cast<uint32_t>(1) << 31;
  if (ind & bit)
  {
    LOG(LINFO, ("hacky"));
    /*
    FeatureType ft(table.m_handle.GetId(), ind ^ bit);
    if (!ft.GetName(StringUtf8Multilang::kDefaultCode, addr.m_street))
    {
      LOG(LWARNING, ("Empty name of the matching street of", bld.m_id));
      return false;
    }
    string name;
    addr.m_building = bld;
    addr.m_street.m_id = ft.m_id;
    addr.m_distanceMeters = 0.0;
    return true;
    */
  }

  LOG(LWARNING, ("Out of bound street index", ind, "for", bld.m_id));
  return false;
}

void ReverseGeocoder::GetNearbyBuildings(m2::PointD const & center, vector<Building> & buildings) const
{
  m2::RectD const rect = GetLookupRect(center, kDefaultLookupRadiusM);

  auto const addBuilding = [&](FeatureType & ft)
  {
    if (!ft.GetHouseNumber().empty())
      buildings.push_back(FromFeature(ft, feature::GetMinDistanceMeters(ft, center)));
  };

  m_index.ForEachInRect(addBuilding, rect, kQueryScale);
  sort(buildings.begin(), buildings.end(), my::CompareBy(&Building::m_distanceMeters));
}

// static
ReverseGeocoder::Building ReverseGeocoder::FromFeature(FeatureType & ft, double distMeters)
{
  return { ft.GetID(), distMeters, ft.GetHouseNumber(), feature::GetCenter(ft) };
}

// static
m2::RectD ReverseGeocoder::GetLookupRect(m2::PointD const & center, double radiusM)
{
  return MercatorBounds::RectByCenterXYAndSizeInMeters(center, radiusM);
}

bool ReverseGeocoder::HouseTable::Get(FeatureID const & fid, uint32_t & streetIndex)
{
  if (!m_table || m_handle.GetId() != fid.m_mwmId)
  {
    m_handle = m_index.GetMwmHandleById(fid.m_mwmId);
    if (!m_handle.IsAlive())
    {
      LOG(LWARNING, ("MWM", fid, "is dead"));
      return false;
    }
    m_table = search::v2::HouseToStreetTable::Load(*m_handle.GetValue<MwmValue>());
  }

  return m_table->Get(fid.m_index, streetIndex);
}

string DebugPrint(ReverseGeocoder::Object const & obj)
{
  return obj.m_name;
}

string DebugPrint(ReverseGeocoder::Address const & addr)
{
  return "{ " + DebugPrint(addr.m_building) + ", " + DebugPrint(addr.m_street) + " }";
}

} // namespace search
