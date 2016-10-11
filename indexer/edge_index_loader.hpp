#pragma once
#include "indexer/feature_edge_index.hpp"
#include "indexer/index.hpp"
#include "indexer/mwm_set.hpp"

// @TODO Move away routing from indexer.
#include "routing/road_graph.hpp"

#include "std/functional.hpp"
#include "std/unordered_map.hpp"
#include "std/shared_ptr.hpp"
#include "std/string.hpp"
#include "std/vector.hpp"

namespace std
{
  template <> struct hash<m2::PointD>
  {
    size_t operator()(m2::PointD const & p) const
    {
      return hash<double>()(p.x) ^ hash<double>()(p.y);
    }
  };
}

namespace feature
{
struct FixEdge
{
  FixEdge(m2::PointD const & startPoint)
    : m_featureId(kInvalidFeatureId), m_segId(0), m_startPoint(startPoint), m_forward(true)
  {
  }

  FixEdge(uint32_t featureId, bool forward, uint32_t segId, m2::PointD const & startPoint,
          m2::PointD const & endPoint)
    : m_featureId(featureId), m_segId(segId), m_startPoint(startPoint), m_endPoint(endPoint),
      m_forward(forward)
  {
  }

  uint32_t m_featureId;
  uint32_t m_segId;
  m2::PointD m_startPoint;
  m2::PointD m_endPoint;
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
  // @TODO |m_outgoingEdges| and |m_ingoingEdgeIndices| could contain more then
  // 10^6 items. Using m2::PointD here it's a waste of memory. m2::PointI should be used.
  vector<FixEdge> m_outgoingEdges; /* sorted by FixEdge::m_startPoint */
  unordered_map<m2::PointD, vector<size_t>> m_ingoingEdgeIndices;

  EdgeIndexHeader m_header;
  string m_countryFileName;
  shared_ptr<MwmInfo> const m_mwmInfo;
};
}  // namespace feature
