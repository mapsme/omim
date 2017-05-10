#pragma once

#include "routing/osrm_engine.hpp"
#include "routing/router.hpp"
#include "routing/routing_mapping.hpp"

#include <string>
#include <vector>

namespace routing
{
/*!
 * \brief The RoutePathCross struct contains information neaded to describe path inside single map.
 */
struct RoutePathCross
{
  FeatureGraphNode startNode; /**< start graph node representation */
  FeatureGraphNode finalNode; /**< end graph node representation */

  RoutePathCross(NodeID const startNodeId, m2::PointD const & startPoint, NodeID const finalNodeId, m2::PointD const & finalPoint, Index::MwmId const & id)
      : startNode(startNodeId, true /* isStartNode */, id),
        finalNode(finalNodeId, false /* isStartNode*/, id)
  {
    startNode.segmentPoint = startPoint;
    finalNode.segmentPoint = finalPoint;
  }
};

using TCheckedPath = std::vector<RoutePathCross>;

/*!
 * \brief CalculateCrossMwmPath function for calculating path through several maps.
 * \param startGraphNodes The std::vector of starting routing graph nodes.
 * \param finalGraphNodes The std::vector of final routing graph nodes.
 * \param route Storage for the result records about crossing maps.
 * \param indexManager Manager for getting indexes of new countries.
 * \param cost Found path cost.
 * \param RoutingVisualizerFn Debug visualization function.
 * \return NoError if the path exists, error code otherwise.
 */
IRouter::ResultCode CalculateCrossMwmPath(TRoutingNodes const & startGraphNodes,
                                          TRoutingNodes const & finalGraphNodes,
                                          RoutingIndexManager & indexManager, double & cost,
                                          RouterDelegate const & delegate, TCheckedPath & route);
}  // namespace routing
