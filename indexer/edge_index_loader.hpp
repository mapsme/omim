#pragma once
#include "indexer/feature_edge_index.hpp"
#include "indexer/index.hpp"
#include "indexer/mwm_set.hpp"

// @TODO Move away routing from indexer.
#include "routing/road_graph.hpp"

#include "std/functional.hpp"
#include "std/unordered_map.hpp"
#include "std/string.hpp"
#include "std/vector.hpp"

namespace std
{
  template <> struct hash<m2::PointI>
  {
    size_t operator()(m2::PointI const & p) const
    {
      return hash<int32_t>()(p.x) ^ hash<int32_t>()(p.y);
    }
  };
}

namespace feature
{
struct FixEdge
{
  FixEdge(m2::PointI const & startPoint)
    : m_featureId(kInvalidFeatureId), m_segId(0), m_startPoint(startPoint), m_forward(true)
  {
  }

  FixEdge(uint32_t featureId, bool forward, uint32_t segId, m2::PointI const & startPoint,
          m2::PointI const & endPoint)
    : m_featureId(featureId), m_segId(segId), m_startPoint(startPoint), m_endPoint(endPoint),
      m_forward(forward)
  {
  }

  uint32_t m_featureId;
  uint32_t m_segId;
  m2::PointI m_startPoint;
  m2::PointI m_endPoint;
  bool m_forward;
};

class EdgeIndexLoader
{
public:
  EdgeIndexLoader(MwmValue const & mwmValue, MwmSet::MwmId const & mwmId);

  // @TODO Now the algorithm uses double point Junction for route calculation.
  // It's worth rewriting it to use fixed point Junction in it.
  bool GetOutgoingEdges(routing::Junction const & junction,
                        routing::IRoadGraph::TEdgeVector & edges) const;
  bool GetIngoingEdges(routing::Junction const & junction,
                       routing::IRoadGraph::TEdgeVector & edges) const;
  bool HasEdgeIndex() const { return !m_outgoingEdges.empty(); }

private:
  vector<FixEdge> m_outgoingEdges; /* sorted by FixEdge::m_startPoint */
  unordered_map<m2::PointI, vector<size_t>> m_ingoingEdgeIndices;

  EdgeIndexHeader m_header;
  string m_countryFileName;
  MwmSet::MwmId const & m_mwmId;
};
}  // namespace feature
