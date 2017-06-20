#include "routing/subway_graph.hpp"

#include "indexer/ftypes_matcher.hpp"
#include "indexer/scales.hpp"

#include "geometry/mercator.hpp"

#include "base/macros.hpp"

#include <limits>

namespace
{
double constexpr kLineSearchRadiusMeters = 100.0;
}  // namespace

namespace routing
{
void SubwayGraph::GetOutgoingEdgesList(TVertexType const & segment, vector<TEdgeType> & edges)
{
  NOTIMPLEMENTED();
}

void SubwayGraph::GetIngoingEdgesList(TVertexType const & segment, vector<TEdgeType> & edges)
{
  NOTIMPLEMENTED();
}

double SubwayGraph::HeuristicCostEstimate(TVertexType const & from, TVertexType const & to)
{
  return 0.0;
}

SubwayVertex SubwayGraph::GetNearestStation(m2::PointD const & point) const
{
  SubwayVertex closestVertex;
  double closestVertexDist = std::numeric_limits<double>::max();

  auto const f = [&](FeatureType & ft)
  {
    if (!m_model->IsRoad(ft))
      return;

    if (!ftypes::IsSubwayLineChecker::Instance()(ft))
      return;

    double const speedKMPH = m_model->GetSpeed(ft);
    if (speedKMPH <= 0.0)
      return;

    // @TODO It's necessary to cache the parsed geometry.
    ft.ParseGeometry(FeatureType::BEST_GEOMETRY);
    size_t const pointsCount = ft.GetPointsCount();
    for (size_t i = 0; i < pointsCount; ++i)
    {
      double const dist = MercatorBounds::DistanceOnEarth(point, ft.GetPoint(i));
      if (dist < closestVertexDist)
      {
        closestVertexDist = dist;
        closestVertex = SubwayVertex(
            m_numMwmIds->GetId(ft.GetID().m_mwmId.GetInfo()->GetLocalFile().GetCountryFile()),
            ft.GetID().m_index, static_cast<uint32_t >(i), SubwayType::Line);
      }
    }
  };

  m_index.ForEachInRect(
    f, MercatorBounds::RectByCenterXYAndSizeInMeters(point, kLineSearchRadiusMeters),
    scales::GetUpperScale());

  ASSERT_NOT_EQUAL(closestVertexDist, std::numeric_limits<double>::max(), ());
  return closestVertex;
}
}  // namespace routing
