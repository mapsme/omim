#pragma once

#include "routing/index_graph_starter.hpp"
#include "routing/num_mwm_id.hpp"
#include "routing/road_graph.hpp"
#include "routing/segment.hpp"

#include "indexer/index.hpp"

#include <map>
#include <vector>

namespace routing
{
class IndexRoadGraph : public RoadGraphBase
{
public:
  IndexRoadGraph(shared_ptr<NumMwmIds> numMwmIds, IndexGraphStarter & starter,
                 std::vector<Segment> const & segments, std::vector<Junction> const & junctions,
                 Index & index);

  // IRoadGraphBase overrides:
  virtual void GetOutgoingEdges(Junction const & junction, TEdgeVector & edges) const override;
  virtual void GetIngoingEdges(Junction const & junction, TEdgeVector & edges) const override;
  virtual double GetMaxSpeedKMPH() const override;
  virtual void GetEdgeTypes(Edge const & edge, feature::TypesHolder & types) const override;
  virtual void GetJunctionTypes(Junction const & junction,
                                feature::TypesHolder & types) const override;

private:
  void GetEdges(Junction const & junction, bool isOutgoing, TEdgeVector & edges) const;
  Junction GetJunction(Segment const & segment, bool front) const;
  std::vector<Segment> const & GetSegments(Junction const & junction, bool isOutgoing) const;

  Index & m_index;
  shared_ptr<NumMwmIds> m_numMwmIds;
  IndexGraphStarter & m_starter;
  std::map<Junction, std::vector<Segment>> m_beginToSegment;
  std::map<Junction, std::vector<Segment>> m_endToSegment;
};
}  // namespace routing
