#pragma once

#include "routing/osrm2feature_map.hpp"
#include "routing/osrm_data_facade.hpp"

#include "indexer/index.hpp"

#include "geometry/point2d.hpp"

#include <vector>

#include "3party/osrm/osrm-backend/data_structures/query_edge.hpp"

namespace routing
{
/// Single graph node representation for routing task
struct FeatureGraphNode
{
  PhantomNode node;
  OsrmMappingTypes::FtSeg segment;
  m2::PointD segmentPoint;
  Index::MwmId mwmId;

  /*!
  * \brief fill FeatureGraphNode with values.
  * \param nodeId osrm node idetifier.
  * \param isStartNode true if this node will first in the path.
  * \param mwmName @nodeId refers node on the graph of this map.
  */
  FeatureGraphNode(NodeID const nodeId, bool const isStartNode, Index::MwmId const & id);

  FeatureGraphNode(NodeID const nodeId, NodeID const reverseNodeId, bool const isStartNode,
                   Index::MwmId const & id);

  /// \brief Invalid graph node constructor
  FeatureGraphNode();
};

/*!
 * \brief The RawPathData struct is our representation of OSRM PathData struct.
 * I use it for excluding dependency from OSRM. Contains OSRM node ID and it's weight.
 */
struct RawPathData
{
  NodeID node;
  EdgeWeight segmentWeight;  // Time in tenths of a second to pass |node|.

  RawPathData() : node(SPECIAL_NODEID), segmentWeight(INVALID_EDGE_WEIGHT) {}

  RawPathData(NodeID node, EdgeWeight segmentWeight)
      : node(node), segmentWeight(segmentWeight)
  {
  }
};

/*!
 * \brief The OSRM routing result struct. Contains the routing result, it's cost and source and
 * target edges.
 * \property shortestPathLength Length of a founded route.
 * \property unpackedPathSegments Segments of a founded route.
 * \property sourceEdge Source graph node of a route.
 * \property targetEdge Target graph node of a route.
 */
struct RawRoutingResult
{
  int shortestPathLength;
  std::vector<std::vector<RawPathData>> unpackedPathSegments;
  FeatureGraphNode sourceEdge;
  FeatureGraphNode targetEdge;
};

//@todo (dragunov) make proper name
using TRoutingNodes = std::vector<FeatureGraphNode>;
using TRawDataFacade = OsrmRawDataFacade<QueryEdge::EdgeData>;

/*!
   * \brief FindWeightsMatrix Find weights matrix from sources to targets. WARNING it finds only
 * weights, not pathes.
   * \param sources Sources graph nodes vector. Each source is the representation of a start OSRM
 * node.
   * \param targets Targets graph nodes vector. Each target is the representation of a finish OSRM
 * node.
   * \param facade Osrm data facade reference.
   * \param packed Result std::vector with weights. Source nodes are rows.
   * cost(source1 -> target1) cost(source1 -> target2) cost(source2 -> target1) cost(source2 ->
 * target2)
   */
void FindWeightsMatrix(TRoutingNodes const & sources, TRoutingNodes const & targets,
                       TRawDataFacade & facade, std::vector<EdgeWeight> & result);

/*! Finds single shortest path in a single MWM between 2 OSRM nodes
   * \param source Source OSRM graph node to make path.
   * \param taget Target OSRM graph node to make path.
   * \param facade OSRM routing data facade to recover graph information.
   * \param rawRoutingResult Routing result structure.
   * \return true when path exists, false otherwise.
   */
bool FindSingleRoute(FeatureGraphNode const & source, FeatureGraphNode const & target,
                     TRawDataFacade & facade, RawRoutingResult & rawRoutingResult);

}  // namespace routing
