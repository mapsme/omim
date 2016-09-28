#pragma once
#include "indexer/feature_edge_index.hpp"
#include "indexer/index.hpp"
#include "indexer/mwm_set.hpp"

// @TODO Move away routing from indexer.
#include "routing/road_graph.hpp"

#include "std/map.hpp"

namespace feature
{
class EdgeIndexLoader
{
public:
  explicit EdgeIndexLoader(MwmValue const & mwmValue, MwmSet::MwmId const & mwmId);

  bool GetOutgoingEdges(routing::Junction const & junction,
                        routing::IRoadGraph::TEdgeVector & edges) const;
  bool GetIngoingEdges(routing::Junction const & junction,
                       routing::IRoadGraph::TEdgeVector & edges) const;
  bool HasEdgeIndex() const { return !m_outgoingEdges.empty() /* && m_ingoingEdges.empty()*/; }

private:
  // @TODO Probably it's better to have a hash table here instead of sorted vector.
  vector<PointOutgoingEdges> m_outgoingEdges; /* sorted with OutgoingEdgeSortFunc */
//  vector<PointOutgoingEdges> m_ingoingEdges;
  EdgeIndexHeader m_header;
  string m_countryFileName;
  MwmSet::MwmId const & m_mwmId;
};
}  // namespace feature
