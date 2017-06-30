#include "routing/road_graph.hpp"
#include "routing/road_graph_router.hpp"

#include "routing/route.hpp"

#include "geometry/mercator.hpp"

#include "geometry/distance_on_sphere.hpp"
#include "geometry/segment2d.hpp"

#include "base/assert.hpp"
#include "base/math.hpp"
#include "base/stl_helpers.hpp"

#include "std/algorithm.hpp"
#include "std/limits.hpp"
#include "std/sstream.hpp"

namespace routing
{
namespace
{
bool OnEdge(Junction const & p, Edge const & ab)
{
  auto const & a = ab.GetStartJunction();
  auto const & b = ab.GetEndJunction();
  return m2::IsPointOnSegmentEps(p.GetPoint(), a.GetPoint(), b.GetPoint(), 1e-9);
}

void SplitEdge(Edge const & ab, Junction const & p, vector<Edge> & edges)
{
  auto const & a = ab.GetStartJunction();
  auto const & b = ab.GetEndJunction();
  bool const partOfReal = ab.IsPartOfReal();
  edges.push_back(Edge::MakeFake(a, p, partOfReal));
  edges.push_back(Edge::MakeFake(p, b, partOfReal));
}
}  // namespace

// Junction --------------------------------------------------------------------

Junction::Junction() : m_point(m2::PointD::Zero()), m_altitude(feature::kDefaultAltitudeMeters) {}
Junction::Junction(m2::PointD const & point, feature::TAltitude altitude)
  : m_point(point), m_altitude(altitude)
{}

string DebugPrint(Junction const & r)
{
  ostringstream ss;
  ss << "Junction{point:" << DebugPrint(r.m_point) << ", altitude:" << r.GetAltitude() << "}";
  return ss.str();
}

// Edge ------------------------------------------------------------------------

Edge Edge::MakeFake(Junction const & startJunction, Junction const & endJunction, bool partOfReal)
{
  Edge edge(FeatureID(), true /* forward */, 0 /* segId */, startJunction, endJunction);
  edge.m_partOfReal = partOfReal;
  return edge;
}

Edge::Edge() : m_forward(true), m_segId(0) { m_partOfReal = !IsFake(); }

Edge::Edge(FeatureID const & featureId, bool forward, uint32_t segId,
           Junction const & startJunction, Junction const & endJunction)
  : m_featureId(featureId)
  , m_forward(forward)
  , m_segId(segId)
  , m_startJunction(startJunction)
  , m_endJunction(endJunction)
{
  ASSERT_LESS(segId, numeric_limits<uint32_t>::max(), ());
  m_partOfReal = !IsFake();
}

Edge Edge::GetReverseEdge() const
{
  Edge edge = *this;
  edge.m_forward = !edge.m_forward;
  swap(edge.m_startJunction, edge.m_endJunction);
  return edge;
}

bool Edge::SameRoadSegmentAndDirection(Edge const & r) const
{
  return m_featureId == r.m_featureId &&
         m_forward == r.m_forward &&
         m_segId == r.m_segId;
}

bool Edge::operator==(Edge const & r) const
{
  return m_featureId == r.m_featureId && m_forward == r.m_forward &&
         m_partOfReal == r.m_partOfReal && m_segId == r.m_segId &&
         m_startJunction == r.m_startJunction && m_endJunction == r.m_endJunction;
}

bool Edge::operator<(Edge const & r) const
{
  if (m_featureId != r.m_featureId)
    return m_featureId < r.m_featureId;
  if (m_forward != r.m_forward)
    return (m_forward == false);
  if (m_partOfReal != r.m_partOfReal)
    return (m_partOfReal == false);
  if (m_segId != r.m_segId)
    return m_segId < r.m_segId;
  if (!(m_startJunction == r.m_startJunction))
    return m_startJunction < r.m_startJunction;
  if (!(m_endJunction == r.m_endJunction))
    return m_endJunction < r.m_endJunction;
  return false;
}

string DebugPrint(Edge const & r)
{
  ostringstream ss;
  ss << "Edge{featureId: " << DebugPrint(r.GetFeatureId()) << ", isForward:" << r.IsForward()
     << ", partOfReal:" << r.IsPartOfReal() << ", segId:" << r.m_segId
     << ", startJunction:" << DebugPrint(r.m_startJunction)
     << ", endJunction:" << DebugPrint(r.m_endJunction) << "}";
  return ss.str();
}

// IRoadGraph::RoadInfo --------------------------------------------------------

IRoadGraph::RoadInfo::RoadInfo()
  : m_speedKMPH(0.0), m_bidirectional(false)
{}
IRoadGraph::RoadInfo::RoadInfo(RoadInfo && ri)
  : m_junctions(move(ri.m_junctions))
  , m_speedKMPH(ri.m_speedKMPH)
  , m_bidirectional(ri.m_bidirectional)
{}

IRoadGraph::RoadInfo::RoadInfo(bool bidirectional, double speedKMPH,
                               initializer_list<Junction> const & points)
  : m_junctions(points), m_speedKMPH(speedKMPH), m_bidirectional(bidirectional)
{}

// IRoadGraph::CrossOutgoingLoader ---------------------------------------------
void IRoadGraph::CrossOutgoingLoader::LoadEdges(FeatureID const & featureId, RoadInfo const & roadInfo)
{
  ForEachEdge(roadInfo, [&featureId, &roadInfo, this](size_t segId, Junction const & endJunction,
                                                      bool forward) {
    if (forward || roadInfo.m_bidirectional || m_mode == IRoadGraph::Mode::IgnoreOnewayTag)
      m_edges.emplace_back(featureId, forward, segId, m_cross, endJunction);
  });
}

// IRoadGraph::CrossIngoingLoader ----------------------------------------------
void IRoadGraph::CrossIngoingLoader::LoadEdges(FeatureID const & featureId, RoadInfo const & roadInfo)
{
  ForEachEdge(roadInfo, [&featureId, &roadInfo, this](size_t segId, Junction const & endJunction,
                                                      bool forward) {
    if (!forward || roadInfo.m_bidirectional || m_mode == IRoadGraph::Mode::IgnoreOnewayTag)
      m_edges.emplace_back(featureId, !forward, segId, endJunction, m_cross);
  });
}

// IRoadGraph ------------------------------------------------------------------
void IRoadGraph::GetOutgoingEdges(Junction const & junction, TEdgeVector & edges) const
{
  GetFakeOutgoingEdges(junction, edges);
  GetRegularOutgoingEdges(junction, edges);
}

void IRoadGraph::GetIngoingEdges(Junction const & junction, TEdgeVector & edges) const
{
  GetFakeIngoingEdges(junction, edges);
  GetRegularIngoingEdges(junction, edges);
}

void IRoadGraph::GetRegularOutgoingEdges(Junction const & junction, TEdgeVector & edges) const
{
  CrossOutgoingLoader loader(junction, GetMode(), edges);
  ForEachFeatureClosestToCross(junction.GetPoint(), loader);
}

void IRoadGraph::GetRegularIngoingEdges(Junction const & junction, TEdgeVector & edges) const
{
  CrossIngoingLoader loader(junction, GetMode(), edges);
  ForEachFeatureClosestToCross(junction.GetPoint(), loader);
}

void IRoadGraph::GetFakeOutgoingEdges(Junction const & junction, TEdgeVector & edges) const
{
  auto const it = m_fakeOutgoingEdges.find(junction);
  if (it != m_fakeOutgoingEdges.cend())
    edges.insert(edges.end(), it->second.cbegin(), it->second.cend());
}

void IRoadGraph::GetFakeIngoingEdges(Junction const & junction, TEdgeVector & edges) const
{
  auto const it = m_fakeIngoingEdges.find(junction);
  if (it != m_fakeIngoingEdges.cend())
    edges.insert(edges.end(), it->second.cbegin(), it->second.cend());
}

void IRoadGraph::ResetFakes()
{
  m_fakeOutgoingEdges.clear();
  m_fakeIngoingEdges.clear();
}

void IRoadGraph::AddFakeEdges(Junction const & junction,
                              vector<pair<Edge, Junction>> const & vicinity)
{
  for (auto const & v : vicinity)
  {
    Edge const & ab = v.first;
    Junction const p = v.second;

    vector<Edge> edges;
    SplitEdge(ab, p, edges);

    edges.push_back(Edge::MakeFake(junction, p, false /* partOfReal */));
    edges.push_back(Edge::MakeFake(p, junction, false /* partOfReal */));

    ForEachFakeEdge([&](Edge const & uv)
                    {
                      if (OnEdge(p, uv))
                        SplitEdge(uv, p, edges);
                    });

    for (auto const & uv : edges)
    {
      auto const & u = uv.GetStartJunction();
      auto const & v = uv.GetEndJunction();
      m_fakeOutgoingEdges[u].push_back(uv);
      m_fakeIngoingEdges[v].push_back(uv);
    }
  }

  for (auto & m : m_fakeIngoingEdges)
    my::SortUnique(m.second);
  for (auto & m : m_fakeOutgoingEdges)
    my::SortUnique(m.second);
}

double IRoadGraph::GetSpeedKMPH(Edge const & edge) const
{
  double const speedKMPH = (edge.IsFake() ? GetMaxSpeedKMPH() : GetSpeedKMPH(edge.GetFeatureId()));
  ASSERT(speedKMPH <= GetMaxSpeedKMPH(), ());
  return speedKMPH;
}

void IRoadGraph::GetEdgeTypes(Edge const & edge, feature::TypesHolder & types) const
{
  if (edge.IsFake())
    types = feature::TypesHolder(feature::GEOM_LINE);
  else
    GetFeatureTypes(edge.GetFeatureId(), types);
}

string DebugPrint(IRoadGraph::Mode mode)
{
  switch (mode)
  {
    case IRoadGraph::Mode::ObeyOnewayTag: return "ObeyOnewayTag";
    case IRoadGraph::Mode::IgnoreOnewayTag: return "IgnoreOnewayTag";
  }
}

IRoadGraph::RoadInfo MakeRoadInfoForTesting(bool bidirectional, double speedKMPH,
                                            initializer_list<m2::PointD> const & points)
{
  IRoadGraph::RoadInfo ri(bidirectional, speedKMPH, {});
  for (auto const & p : points)
    ri.m_junctions.emplace_back(MakeJunctionForTesting(p));

  return ri;
}
// RoadGraphBase ------------------------------------------------------------------
bool RoadGraphBase::IsRouteEdgesImplemented() const
{
  return false;
}

void RoadGraphBase::GetRouteEdges(TEdgeVector & routeEdges) const
{
  NOTIMPLEMENTED()
}
}  // namespace routing
