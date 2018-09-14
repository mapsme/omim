#include "search/street_vicinity_loader.hpp"

#include "indexer/cell_id.hpp"
#include "indexer/feature_covering.hpp"
#include "indexer/feature_decl.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"

#include "base/math.hpp"
#include "base/stl_helpers.hpp"

namespace search
{
StreetVicinityLoader::StreetVicinityLoader(int scale, double offsetMeters)
  : m_context(nullptr), m_scale(scale), m_offsetMeters(offsetMeters), m_cache("Streets")
{
}

void StreetVicinityLoader::SetContext(MwmContext * context)
{
  ASSERT(context, ());
  if (m_context == context)
    return;

  m_context = context;
  auto const scaleRange = m_context->m_value.GetHeader().GetScaleRange();
  m_scale = my::clamp(m_scale, scaleRange.first, scaleRange.second);
}

void StreetVicinityLoader::OnQueryFinished() { m_cache.ClearIfNeeded(); }

StreetVicinityLoader::Street const & StreetVicinityLoader::GetStreet(uint32_t featureId)
{
  auto r = m_cache.Get(featureId);
  if (!r.second)
    return r.first;

  LoadStreet(featureId, r.first);
  return r.first;
}

void StreetVicinityLoader::LoadStreet(uint32_t featureId, Street & street)
{
  FeatureType feature;
  if (!m_context->GetFeature(featureId, feature))
    return;

  if (feature.GetFeatureType() != feature::GEOM_LINE)
    return;

  vector<m2::PointD> points;
  feature.ForEachPoint(base::MakeBackInsertFunctor(points), FeatureType::BEST_GEOMETRY);
  ASSERT(!points.empty(), ());

  for (auto const & point : points)
    street.m_rect.Add(MercatorBounds::RectByCenterXYAndSizeInMeters(point, m_offsetMeters));

  covering::CoveringGetter coveringGetter(street.m_rect, covering::ViewportWithLowLevels);
  auto const & intervals = coveringGetter.Get<RectId::DEPTH_LEVELS>(m_scale);
  m_context->ForEachIndex(intervals, m_scale, base::MakeBackInsertFunctor(street.m_features));

  street.m_calculator = make_unique<ProjectionOnStreetCalculator>(points);
}

}  // namespace search
