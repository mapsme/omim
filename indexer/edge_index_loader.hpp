#pragma once
#include "indexer/feature_edge_index.hpp"
#include "indexer/index.hpp"

#include "routing/road_graph.hpp"

#include "std/map.hpp"

namespace feature
{
class EdgeIndexLoader
{
public:
  explicit EdgeIndexLoader(MwmValue const & mwmValue);

  void GetOutgoingEdges(routing::Junction const & junction,
                        routing::IRoadGraph::TEdgeVector & edges) const;
  void GetIngoingEdges(routing::Junction const & junction,
                       routing::IRoadGraph::TEdgeVector & edges) const;
  bool HasEdgeIndex() const { return m_outgoingEdges.empty() /* && m_ingoingEdges.empty()*/; }

private:
  vector<PointOutgoingEdges> m_outgoingEdges; /* sorted with OutgoingEdgeSortFunc */
//  vector<PointOutgoingEdges> m_ingoingEdges;
  EdgeIndexHeader m_header;
  string m_countryFileName;
};
}  // namespace feature
