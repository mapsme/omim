#pragma once

#include "routing/directions_engine.hpp"
#include "routing/road_graph.hpp"
#include "routing/router.hpp"
#include "routing/routing_algorithm.hpp"

#include "routing_common/vehicle_model.hpp"

#include "indexer/mwm_set.hpp"

#include "geometry/point2d.hpp"

#include <functional>
#include <string>
#include <memory>
#include <vector>

class Index;

namespace routing
{

class RoadGraphRouter : public IRouter
{
public:
  RoadGraphRouter(std::string const & name, Index const & index, TCountryFileFn const & countryFileFn,
                  IRoadGraph::Mode mode, std::unique_ptr<VehicleModelFactory> && vehicleModelFactory,
                  std::unique_ptr<IRoutingAlgorithm> && algorithm,
                  std::unique_ptr<IDirectionsEngine> && directionsEngine);
  ~RoadGraphRouter() override;

  // IRouter overrides:
  std::string GetName() const override { return m_name; }
  void ClearState() override;
  ResultCode CalculateRoute(m2::PointD const & startPoint, m2::PointD const & startDirection,
                            m2::PointD const & finalPoint, RouterDelegate const & delegate,
                            Route & route) override;

private:
  /// Checks existance and add absent maps to route.
  /// Returns true if map exists
  bool CheckMapExistence(m2::PointD const & point, Route & route) const;

  std::string const m_name;
  TCountryFileFn const m_countryFileFn;
  Index const & m_index;
  std::unique_ptr<IRoutingAlgorithm> const m_algorithm;
  std::unique_ptr<IRoadGraph> const m_roadGraph;
  std::unique_ptr<IDirectionsEngine> const m_directionsEngine;
};

unique_ptr<IRouter> CreatePedestrianAStarRouter(Index & index, TCountryFileFn const & countryFileFn);
unique_ptr<IRouter> CreatePedestrianAStarBidirectionalRouter(Index & index, TCountryFileFn const & countryFileFn);
unique_ptr<IRouter> CreateBicycleAStarBidirectionalRouter(Index & index, TCountryFileFn const & countryFileFn);
}  // namespace routing
