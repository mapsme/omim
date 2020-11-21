#pragma once

#include "routing/edge_estimator.hpp"
#include "routing/index_graph.hpp"
#include "routing/route.hpp"
#include "routing/vehicle_mask.hpp"

#include "routing_common/num_mwm_id.hpp"
#include "routing_common/vehicle_model.hpp"

#include <memory>
#include <vector>

class MwmValue;
class DataSource;

namespace routing
{
class IndexGraphLoader
{
public:
  virtual ~IndexGraphLoader() = default;

  virtual Geometry & GetGeometry(NumMwmId numMwmId) = 0;
  virtual IndexGraph & GetIndexGraph(NumMwmId mwmId) = 0;

  // Because several cameras can lie on one segment we return vector of them.
  virtual std::vector<RouteSegment::SpeedCamera> GetSpeedCameraInfo(Segment const & segment) = 0;
  virtual void Clear() = 0;
  virtual bool IsTwoThreadsReady() const = 0;

  static std::unique_ptr<IndexGraphLoader> Create(
      VehicleType vehicleType, bool loadAltitudes, bool twoThreadsReady,
      std::shared_ptr<NumMwmIds> numMwmIds,
      std::shared_ptr<VehicleModelFactoryInterface> vehicleModelFactory,
      std::shared_ptr<EdgeEstimator> estimator, DataSource & dataSource,
      RoutingOptions routingOptions = RoutingOptions());
};

void DeserializeIndexGraph(MwmValue const & mwmValue, VehicleType vehicleType, IndexGraph & graph);
}  // namespace routing
