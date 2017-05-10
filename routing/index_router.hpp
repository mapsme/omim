#pragma once

#include "routing/cross_mwm_graph.hpp"
#include "routing/directions_engine.hpp"
#include "routing/edge_estimator.hpp"
#include "routing/features_road_graph.hpp"
#include "routing/joint.hpp"
#include "routing/num_mwm_id.hpp"
#include "routing/router.hpp"
#include "routing/routing_mapping.hpp"

#include "routing_common/vehicle_model.hpp"

#include "indexer/index.hpp"
#include "indexer/mwm_set.hpp"

#include "geometry/tree4d.hpp"

#include <memory>
#include <memory>
#include <vector>

namespace routing
{
class IndexGraph;
class IndexGraphStarter;

class IndexRouter : public IRouter
{
public:
  IndexRouter(std::string const & name, TCountryFileFn const & countryFileFn,
              CourntryRectFn const & countryRectFn, std::shared_ptr<NumMwmIds> numMwmIds,
              std::unique_ptr<m4::Tree<NumMwmId>> numMwmTree, std::shared_ptr<TrafficStash> trafficStash,
              std::shared_ptr<VehicleModelFactory> vehicleModelFactory,
              std::shared_ptr<EdgeEstimator> estimator, std::unique_ptr<IDirectionsEngine> directionsEngine,
              Index & index);

  // IRouter overrides:
  virtual std::string GetName() const override { return m_name; }
  virtual IRouter::ResultCode CalculateRoute(m2::PointD const & startPoint,
                                             m2::PointD const & startDirection,
                                             m2::PointD const & finalPoint,
                                             RouterDelegate const & delegate,
                                             Route & route) override;

  IRouter::ResultCode CalculateRouteForSingleMwm(std::string const & country,
                                                 m2::PointD const & startPoint,
                                                 m2::PointD const & startDirection,
                                                 m2::PointD const & finalPoint,
                                                 RouterDelegate const & delegate, Route & route);

  /// \note |numMwmIds| should not be null.
  static std::unique_ptr<IndexRouter> CreateCarRouter(TCountryFileFn const & countryFileFn,
                                                 CourntryRectFn const & coutryRectFn,
                                                 std::shared_ptr<NumMwmIds> numMwmIds,
                                                 std::unique_ptr<m4::Tree<NumMwmId>> numMwmTree,
                                                 traffic::TrafficCache const & trafficCache,
                                                 Index & index);

private:
  IRouter::ResultCode CalculateRoute(std::string const & startCountry, std::string const & finishCountry,
                                     bool forSingleMwm, m2::PointD const & startPoint,
                                     m2::PointD const & startDirection,
                                     m2::PointD const & finalPoint, RouterDelegate const & delegate,
                                     Route & route);
  IRouter::ResultCode DoCalculateRoute(std::string const & startCountry, std::string const & finishCountry,
                                       bool forSingleMwm, m2::PointD const & startPoint,
                                       m2::PointD const & startDirection,
                                       m2::PointD const & finalPoint,
                                       RouterDelegate const & delegate, Route & route);
  bool FindClosestEdge(platform::CountryFile const & file, m2::PointD const & point,
                       Edge & closestEdge) const;
  // Input route may contains 'leaps': shortcut edges from mwm border enter to exit.
  // ProcessLeaps replaces each leap with calculated route through mwm.
  IRouter::ResultCode ProcessLeaps(std::vector<Segment> const & input, RouterDelegate const & delegate,
                                   IndexGraphStarter & starter, std::vector<Segment> & output);
  bool RedressRoute(std::vector<Segment> const & segments, RouterDelegate const & delegate,
                    bool forSingleMwm, IndexGraphStarter & starter, Route & route) const;

  bool AreMwmsNear(NumMwmId startId, NumMwmId finishId) const;

  std::string const m_name;
  Index & m_index;
  TCountryFileFn const m_countryFileFn;
  CourntryRectFn const m_countryRectFn;
  std::shared_ptr<NumMwmIds> m_numMwmIds;
  std::shared_ptr<m4::Tree<NumMwmId>> m_numMwmTree;
  std::shared_ptr<TrafficStash> m_trafficStash;
  RoutingIndexManager m_indexManager;
  FeaturesRoadGraph m_roadGraph;
  std::shared_ptr<VehicleModelFactory> m_vehicleModelFactory;
  std::shared_ptr<EdgeEstimator> m_estimator;
  std::unique_ptr<IDirectionsEngine> m_directionsEngine;
};
}  // namespace routing
