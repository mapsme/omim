#pragma once

#include "routing/num_mwm_id.hpp"
#include "routing/router.hpp"
#include "routing/subway_graph.hpp"
#include "routing/subway_model.hpp"
#include "routing/subway_vertex.hpp"

#include "routing_common/vehicle_model.hpp"

#include "indexer/index.hpp"

#include <memory>
#include <vector>

namespace routing
{
class SubwayRouter final : public IRouter
{
public:
  SubwayRouter(std::shared_ptr<NumMwmIds> numMwmIds, Index & index);

  // IRouter overrides:
  virtual std::string GetName() const override { return "subway"; }
  virtual IRouter::ResultCode CalculateRoute(m2::PointD const & startPoint,
                                             m2::PointD const & startDirection,
                                             m2::PointD const & finalPoint,
                                             RouterDelegate const & delegate,
                                             Route & route) override;

private:
  void SetGeometry(std::vector<SubwayVertex> const & vertexes, Route & route) const;
  void SetTimes(size_t routeSize, double time, Route & route) const;

  Index & m_index;
  std::shared_ptr<NumMwmIds> m_numMwmIds;
  SubwayModelFactory m_modelFactory;
  SubwayGraph m_graph;
};
}  // namespace routing
