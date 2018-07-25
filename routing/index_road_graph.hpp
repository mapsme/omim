#pragma once

#include "routing/index_graph_starter.hpp"
#include "routing/road_graph.hpp"
#include "routing/segment.hpp"

#include "routing_common/num_mwm_id.hpp"

#include "indexer/data_source.hpp"

#include <map>
#include <memory>
#include <vector>

namespace routing
{
class IndexRoadGraph : public RoadGraphBase
{
public:
  IndexRoadGraph(std::shared_ptr<NumMwmIds> numMwmIds, IndexGraphStarter & starter,
                 std::vector<Segment> const & segments, std::vector<Junction> const & junctions,
                 DataSource & dataSource);

  // IRoadGraphBase overrides:
  virtual void GetOutgoingEdges(Junction const & junction, TEdgeVector & edges) const override;
  virtual void GetIngoingEdges(Junction const & junction, TEdgeVector & edges) const override;
  virtual double GetMaxSpeedKMpH() const override;
  virtual void GetEdgeTypes(Edge const & edge, feature::TypesHolder & types) const override;
  virtual void GetJunctionTypes(Junction const & junction,
                                feature::TypesHolder & types) const override;
  virtual void GetRouteEdges(TEdgeVector & edges) const override;
  virtual void GetRouteSegments(std::vector<Segment> & segments) const override;

private:
  void GetEdges(Junction const & junction, bool isOutgoing, TEdgeVector & edges) const;
  std::vector<Segment> const & GetSegments(Junction const & junction, bool isOutgoing) const;

  DataSource & m_dataSource;
  std::shared_ptr<NumMwmIds> m_numMwmIds;
  IndexGraphStarter & m_starter;
  std::vector<Segment> m_segments;
  std::map<Junction, std::vector<Segment>> m_beginToSegment;
  std::map<Junction, std::vector<Segment>> m_endToSegment;
};
}  // namespace routing
