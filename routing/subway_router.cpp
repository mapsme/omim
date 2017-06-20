#include "routing/subway_router.hpp"

#include "routing/base/astar_algorithm.hpp"

using namespace routing;
using namespace std;

namespace
{
IRouter::ResultCode ConvertResult(typename AStarAlgorithm<SubwayGraph>::Result result)
{
  switch (result)
  {
  case AStarAlgorithm<SubwayGraph>::Result::NoPath: return IRouter::RouteNotFound;
  case AStarAlgorithm<SubwayGraph>::Result::Cancelled: return IRouter::Cancelled;
  case AStarAlgorithm<SubwayGraph>::Result::OK: return IRouter::NoError;
  }
}
}

namespace routing
{
SubwayRouter::SubwayRouter(std::shared_ptr<NumMwmIds> numMwmIds, Index & index)
  : m_numMwmIds(std::move(numMwmIds))
  , m_modelFactory(make_shared<SubwayModelFactory>())
  , m_cache(make_shared<SubwayCache>(m_numMwmIds, m_modelFactory, index))
  , m_graph(m_modelFactory, m_numMwmIds, m_cache, index)
{
  CHECK(m_numMwmIds, ());
}

IRouter::ResultCode SubwayRouter::CalculateRoute(m2::PointD const & startPoint,
                                                 m2::PointD const & startDirection,
                                                 m2::PointD const & finalPoint,
                                                 RouterDelegate const & delegate, Route & route)
{
  SubwayVertex start;
  if (!m_graph.GetNearestStation(startPoint, start))
    return IRouter::StartPointNotFound;

  SubwayVertex finish;
  if (!m_graph.GetNearestStation(finalPoint, finish))
    return IRouter::EndPointNotFound;

  AStarAlgorithm<SubwayGraph> algorithm;
  RoutingResult<SubwayVertex> result;
  auto const status = ConvertResult(algorithm.FindPath(m_graph, start, finish, result, delegate,
                                                       nullptr /* onVisitedVertexCallback */));

  if (status != IRouter::NoError)
    return status;

  CHECK(!result.path.empty(), ());
  SetRouteAttrs(result.path, route);
  SetTimes(result.path.size(), result.distance, route);
  return IRouter::NoError;
}

void SubwayRouter::SetRouteAttrs(vector<SubwayVertex> const &vertexes, Route &route) const
{
  CHECK(!vertexes.empty(), ());

  vector<m2::PointD> points;
  points.reserve(vertexes.size());

  vector<string> colors;
  colors.reserve(vertexes.size() - 1);

  auto constexpr invalidFeatureId = numeric_limits<uint32_t>::max();
  uint32_t prevFeatureId = invalidFeatureId;
  SubwayType prevType = SubwayType::Change;

  for (SubwayVertex const & vertex : vertexes)
  {
    auto const featureId = vertex.GetFeatureId();
    SubwayFeature const & feature = m_cache->GetFeature(vertex.GetMwmId(), featureId);

    CHECK_LESS(vertex.GetPointId(), feature.GetPoints().size(), ());
    points.push_back(feature.GetPoints()[vertex.GetPointId()]);

    if (prevFeatureId != invalidFeatureId)
    {
      bool const isColored = prevType == SubwayType::Line && vertex.GetType() == SubwayType::Line &&
                             prevFeatureId == featureId;
      string const color = isColored ? feature.GetColor() : "";
      colors.push_back(color);
    }

    prevFeatureId = featureId;
    prevType = vertex.GetType();
  }

  CHECK_EQUAL(colors.size(), points.size() - 1, ());

  route.SetGeometry(points.cbegin(), points.cend());
  route.SetColors(move(colors));
}

void SubwayRouter::SetTimes(size_t routeSize, double time, Route & route) const
{
  // TODO - all times is final time. Calculate fair times.
  Route::TTimes times;
  times.reserve(routeSize);
  for (uint32_t i = 0; i < static_cast<uint32_t>(routeSize); ++i)
    times.emplace_back(i, time);

  route.SetSectionTimes(move(times));
}
}  // namespace routing
