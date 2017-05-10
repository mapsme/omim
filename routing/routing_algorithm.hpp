#pragma once

#include "base/cancellable.hpp"

#include "routing/road_graph.hpp"
#include "routing/router.hpp"
#include "routing/base/astar_algorithm.hpp"

#include <functional>
#include <string>
#include <vector>

namespace routing
{

// IRoutingAlgorithm is an abstract interface of a routing algorithm,
// which searches the optimal way between two junctions on the graph
class IRoutingAlgorithm
{
public:
  enum class Result
  {
    OK,
    NoPath,
    Cancelled
  };

  virtual Result CalculateRoute(IRoadGraph const & graph, Junction const & startPos,
                                Junction const & finalPos, RouterDelegate const & delegate,
                                RoutingResult<Junction> & path) = 0;
};

std::string DebugPrint(IRoutingAlgorithm::Result const & result);

// AStar routing algorithm implementation
class AStarRoutingAlgorithm : public IRoutingAlgorithm
{
public:
  // IRoutingAlgorithm overrides:
  Result CalculateRoute(IRoadGraph const & graph, Junction const & startPos,
                        Junction const & finalPos, RouterDelegate const & delegate,
                        RoutingResult<Junction> & path) override;
};

// AStar-bidirectional routing algorithm implementation
class AStarBidirectionalRoutingAlgorithm : public IRoutingAlgorithm
{
public:
  // IRoutingAlgorithm overrides:
  Result CalculateRoute(IRoadGraph const & graph, Junction const & startPos,
                        Junction const & finalPos, RouterDelegate const & delegate,
                        RoutingResult<Junction> & path) override;
};

}  // namespace routing
