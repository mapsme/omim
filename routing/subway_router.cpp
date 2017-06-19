#include "routing/subway_router.hpp"

#include "routing/base/astar_algorithm.hpp"
#include "routing/geometry.hpp"

#include <unordered_map>

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
  : m_index(index), m_numMwmIds(std::move(numMwmIds))
{
  CHECK(m_numMwmIds, ());
}

IRouter::ResultCode SubwayRouter::CalculateRoute(m2::PointD const & startPoint,
                                                 m2::PointD const & startDirection,
                                                 m2::PointD const & finalPoint,
                                                 RouterDelegate const & delegate, Route & route)
{
  auto start = m_graph.GetNearestStation(startPoint);
  auto finish = m_graph.GetNearestStation(finalPoint);

  AStarAlgorithm<SubwayGraph> algorithm;
  RoutingResult<WorldRoadPoint> result;
  auto const status = ConvertResult(algorithm.FindPath(m_graph, start, finish, result, delegate,
                                                       nullptr /* onVisitedVertexCallback */));

  if (status != IRouter::NoError)
    return status;

  CHECK(!result.path.empty(), ());
  SetGeometry(result.path, route);
  SetTimes(result.path.size(), result.distance, route);
  return IRouter::NoError;
}

void SubwayRouter::SetGeometry(vector<WorldRoadPoint> const & vertexes, Route & route) const
{
  vector<m2::PointD> points;
  points.reserve(vertexes.size());

  unordered_map<NumMwmId, unique_ptr<Geometry>> geometries;

  for (WorldRoadPoint const & vertex : vertexes)
  {
    auto const numMwmId = vertex.GetMwmId();

    unique_ptr<Geometry> & geometry = geometries[numMwmId];
    if (!geometry)
    {
      platform::CountryFile const & file = m_numMwmIds->GetFile(numMwmId);
      MwmSet::MwmHandle handle = m_index.GetMwmHandleByCountryFile(file);
      CHECK(handle.IsAlive(), ("Can't get mwm handle for", file));

      auto const mwmId = MwmSet::MwmId(handle.GetInfo());

      geometry = make_unique<Geometry>(GeometryLoader::Create(
          m_index, mwmId,
          m_modelFactory.GetVehicleModelForCountry(m_numMwmIds->GetFile(numMwmId).GetName())));
    }

    CHECK(geometry, ());

    points.push_back(geometry->GetPoint(RoadPoint(vertex.GetFeatureId(), vertex.GetPointId())));
  }

  route.SetGeometry(points.cbegin(), points.cend());
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
