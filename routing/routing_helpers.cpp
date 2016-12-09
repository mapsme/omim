#include "routing/routing_helpers.hpp"
#include "routing/road_point.hpp"

#include "traffic/traffic_info.hpp"

namespace routing
{
using namespace traffic;

void ReconstructRoute(IDirectionsEngine * engine, IRoadGraph const & graph,
                      shared_ptr<traffic::TrafficInfo> trafficInfo,
                      my::Cancellable const & cancellable, vector<Junction> & path, Route & route)
{
  if (path.empty())
  {
    LOG(LERROR, ("Can't reconstruct route from an empty list of positions."));
    return;
  }

  // By some reason there're two adjacent positions on a road with
  // the same end-points. This could happen, for example, when
  // direction on a road was changed.  But it doesn't matter since
  // this code reconstructs only geometry of a route.
  path.erase(unique(path.begin(), path.end()), path.end());

  Route::TTimes times;
  Route::TTurns turnsDir;
  vector<Junction> junctions;
  // @TODO(bykoianko) streetNames is not filled in Generate(). It should be done.
  Route::TStreets streetNames;
  vector<TrafficInfo::RoadSegmentId> routeSegs;
  if (engine)
    engine->Generate(graph, path, times, turnsDir, junctions, routeSegs, cancellable);

  vector<m2::PointD> routeGeometry;
  JunctionsToPoints(junctions, routeGeometry);
  feature::TAltitudes altitudes;
  JunctionsToAltitudes(junctions, altitudes);

  route.SetGeometry(routeGeometry.begin(), routeGeometry.end());
  route.SetSectionTimes(move(times));
  route.SetTurnInstructions(move(turnsDir));
  route.SetStreetNames(move(streetNames));
  route.SetAltitudes(move(altitudes));

  vector<traffic::SpeedGroup> traffic;
  traffic.reserve(routeSegs.size());
  if (trafficInfo)
  {
    for (TrafficInfo::RoadSegmentId const & rp : routeSegs)
    {
      auto const it = trafficInfo->GetColoring().find(rp);
      if (it == trafficInfo->GetColoring().cend())
      {
        LOG(LERROR, ("There's no TrafficInfo::RoadSegmentId", rp, "in traffic info for", trafficInfo->GetMwmId()));
        return;
      }
      // @TODO It's written to compensate an error. The problem is in case of any routing except for osrm
      //
      traffic.push_back(it->second);
      traffic.push_back(it->second);
    }
    if (!traffic.empty())
      traffic.pop_back();
  }

  route.SetTraffic(move(traffic));
}
}  // namespace rouing
