#pragma once

#include "routing/directions_engine.hpp"

#include "routing_common/num_mwm_id.hpp"

#include "geometry/point_with_altitude.hpp"

#include <memory>
#include <vector>

namespace routing
{

class PedestrianDirectionsEngine : public IDirectionsEngine
{
public:
  PedestrianDirectionsEngine(std::shared_ptr<NumMwmIds> numMwmIds);

  // IDirectionsEngine override:
  bool Generate(IndexRoadGraph const & graph, std::vector<geometry::PointWithAltitude> const & path,
                base::Cancellable const & cancellable, Route::TTurns & turns,
                Route::TStreets & streetNames,
                std::vector<geometry::PointWithAltitude> & routeGeometry,
                std::vector<Segment> & segments) override;
  void Clear() override {}

private:
  void CalculateTurns(IndexRoadGraph const & graph, std::vector<Edge> const & routeEdges,
                      Route::TTurns & turnsDir, base::Cancellable const & cancellable) const;

  uint32_t const m_typeSteps;
  uint32_t const m_typeLiftGate;
  uint32_t const m_typeGate;
  std::shared_ptr<NumMwmIds> const m_numMwmIds;
};

}  // namespace routing
