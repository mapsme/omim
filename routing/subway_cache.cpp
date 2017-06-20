#include "routing/subway_cache.hpp"

#include "indexer/ftypes_matcher.hpp"

#include "base/assert.hpp"

using namespace std;

namespace routing
{
SubwayCache::SubwayCache(std::shared_ptr<NumMwmIds> numMwmIds, Index & index)
  : m_numMwmIds(std::move(numMwmIds)), m_index(index)
{
  CHECK(m_numMwmIds, ());
}

SubwayFeature const & SubwayCache::GetFeature(NumMwmId numMwmId, uint32_t featureId)
{
  auto & mwmFeatures = m_features[numMwmId];
  auto it = mwmFeatures.find(featureId);
  if (it != mwmFeatures.end())
    return it->second;
  
  platform::CountryFile const & file = m_numMwmIds->GetFile(numMwmId);
  MwmSet::MwmHandle handle = m_index.GetMwmHandleByCountryFile(file);
  CHECK(handle.IsAlive(), ("Can't get mwm handle for", file.GetName()));

  auto const mwmId = MwmSet::MwmId(handle.GetInfo());
  Index::FeaturesLoaderGuard guard(m_index, mwmId);

  FeatureType feature;
  bool const isFound = guard.GetFeatureByIndex(featureId, feature);
  CHECK(isFound, ("Feature", featureId, "not found in ", file.GetName()));

  feature.ParseMetadata();
  feature.ParseGeometry(FeatureType::BEST_GEOMETRY);

  vector<m2::PointD> points;
  for (size_t i = 0; i < feature.GetPointsCount(); ++i)
    points.push_back(feature.GetPoint(i));

  SubwayType type = SubwayType::Line;
  if (ftypes::IsSubwayLineChecker::Instance()(feature))
    type = SubwayType::Line;
  else if (ftypes::IsSubwayChangeChecker::Instance()(feature))
    type = SubwayType::Change;
  else
    CHECK(false, ("Unknown subway type, mwm:", file.GetName(), ", feature:", featureId));

  string const & color = type == SubwayType::Line
                             ? feature.GetMetadata().Get(feature::Metadata::EType::FMD_COLOUR)
                             : "";

  return mwmFeatures[numMwmId] = SubwayFeature(type, color, move(points));
}
}  // namespace routing
