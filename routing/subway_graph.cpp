#include "routing/subway_graph.hpp"

#include "indexer/ftypes_matcher.hpp"
#include "indexer/scales.hpp"

#include "geometry/mercator.hpp"

#include "base/macros.hpp"

#include <limits>

namespace
{
double constexpr kLineSearchRadiusMeters = 100.0;
double constexpr kEquivalenceDistMeters = 3.0;
}  // namespace

namespace routing
{
void SubwayGraph::GetOutgoingEdgesList(TVertexType const & vertex, vector<TEdgeType> & edges)
{
  // Getting point by |vertex|.
  MwmSet::MwmHandle handle = m_index.GetMwmHandleByCountryFile(m_numMwmIds->GetFile(vertex.GetMwmId()));
  Index::FeaturesLoaderGuard const guard(m_index, handle.GetId());
  FeatureType ft;
  if (!guard.GetFeatureByIndex(vertex.GetFeatureId(), ft))
  {
    LOG(LERROR, ("Feature can't be loaded:", vertex.GetFeatureId()));
    return;
  }
  ft.ParseGeometry(FeatureType::BEST_GEOMETRY);
  CHECK_LESS(vertex.GetPointId(), ft.GetPointsCount(), ());
  m2::PointD const vertexPoint = ft.GetPoint(vertex.GetPointId());

  // Getting outgoing edges by |vertexPoint|.
  auto const f = [&](FeatureType const & ft)
  {
    if (!IsValidRoad(ft))
      return;

    ft.ParseGeometry(FeatureType::BEST_GEOMETRY);
    ft.ParseMetadata();

    size_t const pointsCount = ft.GetPointsCount();
    NumMwmId const ftNumMwmId = m_numMwmIds->GetId(ft.GetID().m_mwmId.GetInfo()->GetLocalFile().GetCountryFile());
    uint32_t const ftFeatureId = ft.GetID().m_index;
    SubwayType const ftType = ftypes::IsSubwayLineChecker::Instance()(ft) ? SubwayType::Line
                                                                          : SubwayType::Change;

    for (size_t i = 0; i < pointsCount; ++i)
    {
      if (MercatorBounds::DistanceOnEarth(vertexPoint, ft.GetPoint(i)) < kEquivalenceDistMeters)
      {
        // @TODO Implement filling |edges| more carefully.
        if (i != 0)
          edges.emplace_back(SubwayVertex(ftNumMwmId, ftFeatureId, i - 1, ftType), 1.0 /* weight */);
        if (i + 1 != pointsCount)
          edges.emplace_back(SubwayVertex(ftNumMwmId, ftFeatureId, i + 1, ftType), 1.0 /* weight */);
      }
    }
  };
  m_index.ForEachInRect(
    f, MercatorBounds::RectByCenterXYAndSizeInMeters(vertexPoint, kEquivalenceDistMeters),
    scales::GetUpperScale());
}

void SubwayGraph::GetIngoingEdgesList(TVertexType const & vertex, vector<TEdgeType> & edges)
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
  double closestVertexDistMeters = std::numeric_limits<double>::max();

  auto const f = [&](FeatureType const & ft)
  {
    if (!IsValidRoad(ft))
      return;

    if (!ftypes::IsSubwayLineChecker::Instance()(ft))
      return;

    // @TODO It's necessary to cache the parsed geometry.
    ft.ParseGeometry(FeatureType::BEST_GEOMETRY);
    size_t const pointsCount = ft.GetPointsCount();
    for (size_t i = 0; i < pointsCount; ++i)
    {
      double const distMeters = MercatorBounds::DistanceOnEarth(point, ft.GetPoint(i));
      if (distMeters < closestVertexDistMeters)
      {
        closestVertexDistMeters = distMeters;
        closestVertex = SubwayVertex(
            m_numMwmIds->GetId(ft.GetID().m_mwmId.GetInfo()->GetLocalFile().GetCountryFile()),
            ft.GetID().m_index, static_cast<uint32_t >(i), SubwayType::Line);
      }
    }
  };

  m_index.ForEachInRect(
    f, MercatorBounds::RectByCenterXYAndSizeInMeters(point, kLineSearchRadiusMeters),
    scales::GetUpperScale());

  CHECK_NOT_EQUAL(closestVertexDistMeters, std::numeric_limits<double>::max(), ());
  return closestVertex;
}

bool SubwayGraph::IsValidRoad(FeatureType const & ft) const
{
  if (!m_modelFactory->GetVehicleModel()->IsRoad(ft))
    return false;

  double const speedKMPH = m_modelFactory->GetVehicleModel()->GetSpeed(ft);
  if (speedKMPH <= 0.0)
    return false;

  return true;
}
}  // namespace routing
