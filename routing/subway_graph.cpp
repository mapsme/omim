#include "routing/subway_graph.hpp"

#include "base/macros.hpp"

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

WorldRoadPoint SubwayGraph::GetNearestStation(m2::PointD const & point) const
{
  NOTIMPLEMENTED();
  return WorldRoadPoint();
}
}  // namespace routing
