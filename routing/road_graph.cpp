#include "routing/road_graph.hpp"

#include "routing/route.hpp"

#include "geometry/distance_on_sphere.hpp"
#include "geometry/mercator.hpp"
#include "geometry/segment2d.hpp"

#include "base/assert.hpp"
#include "base/checked_cast.hpp"
#include "base/math.hpp"
#include "base/stl_helpers.hpp"

#include <algorithm>
#include <limits>
#include <sstream>

using namespace std;

namespace routing
{
namespace
{
bool OnEdge(geometry::PointWithAltitude const & p, Edge const & ab)
{
  auto const & a = ab.GetStartJunction();
  auto const & b = ab.GetEndJunction();
  return m2::IsPointOnSegmentEps(p.GetPoint(), a.GetPoint(), b.GetPoint(), 1e-9);
}

void SplitEdge(Edge const & ab, geometry::PointWithAltitude const & p, vector<Edge> & edges)
{
  auto const & a = ab.GetStartJunction();
  auto const & b = ab.GetEndJunction();

  // No need to split the edge by its endpoints.
  if (a.GetPoint() == p.GetPoint() || b.GetPoint() == p.GetPoint())
    return;

  edges.push_back(Edge::MakeFake(a, p, ab));
  edges.push_back(Edge::MakeFake(p, b, ab));
}
}  // namespace

// Edge ------------------------------------------------------------------------
// static
Edge Edge::MakeReal(FeatureID const & featureId, bool forward, uint32_t segId,
                    geometry::PointWithAltitude const & startJunction,
                    geometry::PointWithAltitude const & endJunction)
{
  return {Type::Real, featureId, kInvalidFakeSegmentId, forward, segId, startJunction, endJunction};
}

// static
Edge Edge::MakeFakeWithRealPart(FeatureID const & featureId, uint32_t fakeSegmentId, bool forward,
                                uint32_t segId, geometry::PointWithAltitude const & startJunction,
                                geometry::PointWithAltitude const & endJunction)
{
  return {Type::FakeWithRealPart, featureId, fakeSegmentId, forward,
          segId, startJunction, endJunction};
}

// static
Edge Edge::MakeFake(geometry::PointWithAltitude const & startJunction,
                    geometry::PointWithAltitude const & endJunction)
{
  return {Type::FakeWithoutRealPart, FeatureID(), kInvalidFakeSegmentId, true /* forward */,
          0 /* segmentId */, startJunction, endJunction};
}

// static
Edge Edge::MakeFake(geometry::PointWithAltitude const & startJunction,
                    geometry::PointWithAltitude const & endJunction, Edge const & prototype)
{
  auto e = prototype;
  e.m_startJunction = startJunction;
  e.m_endJunction = endJunction;
  e.m_type = e.HasRealPart() ? Type::FakeWithRealPart : Type::FakeWithoutRealPart;
  return e;
}

Edge::Edge(Type type, FeatureID const & featureId, uint32_t fakeSegmentId, bool forward,
           uint32_t segId, geometry::PointWithAltitude const & startJunction,
           geometry::PointWithAltitude const & endJunction)
  : m_type(type)
  , m_featureId(featureId)
  , m_forward(forward)
  , m_segId(segId)
  , m_startJunction(startJunction)
  , m_endJunction(endJunction)
  , m_fakeSegmentId(fakeSegmentId)
{
  ASSERT_LESS(segId, numeric_limits<uint32_t>::max(), ());
  ASSERT((m_featureId.IsValid() && HasRealPart()) || (!m_featureId.IsValid() && !HasRealPart()),
         ());
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
  return m_type == r.m_type && m_featureId == r.m_featureId && m_forward == r.m_forward &&
         m_segId == r.m_segId && m_startJunction == r.m_startJunction &&
         m_endJunction == r.m_endJunction;
}

bool Edge::operator<(Edge const & r) const
{
  if (m_type != r.m_type)
    return m_type < r.m_type;
  if (m_featureId != r.m_featureId)
    return m_featureId < r.m_featureId;
  if (m_forward != r.m_forward)
    return (m_forward == false);
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
     << ", partOfReal:" << r.HasRealPart() << ", segId:" << r.m_segId
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
                               initializer_list<geometry::PointWithAltitude> const & points)
  : m_junctions(points), m_speedKMPH(speedKMPH), m_bidirectional(bidirectional)
{}

// IRoadGraph::CrossOutgoingLoader ---------------------------------------------
void IRoadGraph::CrossOutgoingLoader::LoadEdges(FeatureID const & featureId,
                                                PointWithAltitudeVec const & junctions,
                                                bool bidirectional)
{
  ForEachEdge(junctions, [&featureId, bidirectional, this](
                             size_t segId, geometry::PointWithAltitude const & endJunction,
                             bool forward) {
    if (forward || bidirectional || m_mode == IRoadGraph::Mode::IgnoreOnewayTag)
    {
      m_edges.push_back(Edge::MakeReal(featureId, forward, base::asserted_cast<uint32_t>(segId),
                                       m_cross, endJunction));
    }
  });
}

// IRoadGraph::CrossIngoingLoader ----------------------------------------------
void IRoadGraph::CrossIngoingLoader::LoadEdges(FeatureID const & featureId,
                                               PointWithAltitudeVec const & junctions,
                                               bool bidirectional)
{
  ForEachEdge(junctions, [&featureId, bidirectional, this](
                             size_t segId, geometry::PointWithAltitude const & endJunction,
                             bool forward) {
    if (!forward || bidirectional || m_mode == IRoadGraph::Mode::IgnoreOnewayTag)
    {
      m_edges.push_back(Edge::MakeReal(featureId, !forward, base::asserted_cast<uint32_t>(segId),
                                       endJunction, m_cross));
    }
  });
}

// IRoadGraph ------------------------------------------------------------------
void IRoadGraph::GetOutgoingEdges(geometry::PointWithAltitude const & junction,
                                  EdgeVector & edges) const
{
  GetFakeOutgoingEdges(junction, edges);
  GetRegularOutgoingEdges(junction, edges);
}

void IRoadGraph::GetIngoingEdges(geometry::PointWithAltitude const & junction,
                                 EdgeVector & edges) const
{
  GetFakeIngoingEdges(junction, edges);
  GetRegularIngoingEdges(junction, edges);
}

void IRoadGraph::GetRegularOutgoingEdges(geometry::PointWithAltitude const & junction,
                                         EdgeVector & edges) const
{
  CrossOutgoingLoader loader(junction, GetMode(), edges);
  ForEachFeatureClosestToCross(junction.GetPoint(), loader);
}

void IRoadGraph::GetRegularIngoingEdges(geometry::PointWithAltitude const & junction,
                                        EdgeVector & edges) const
{
  CrossIngoingLoader loader(junction, GetMode(), edges);
  ForEachFeatureClosestToCross(junction.GetPoint(), loader);
}

void IRoadGraph::GetFakeOutgoingEdges(geometry::PointWithAltitude const & junction,
                                      EdgeVector & edges) const
{
  auto const it = m_fakeOutgoingEdges.find(junction);
  if (it != m_fakeOutgoingEdges.cend())
    edges.insert(edges.end(), it->second.cbegin(), it->second.cend());
}

void IRoadGraph::GetFakeIngoingEdges(geometry::PointWithAltitude const & junction,
                                     EdgeVector & edges) const
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

void IRoadGraph::AddEdge(geometry::PointWithAltitude const & j, Edge const & e,
                         map<geometry::PointWithAltitude, EdgeVector> & edges)
{
  auto & cont = edges[j];
  ASSERT(is_sorted(cont.cbegin(), cont.cend()), ());
  auto const range = equal_range(cont.cbegin(), cont.cend(), e);
  // Note. The "if" condition below is necessary to prevent duplicates which may be added when
  // edges from |j| to "projection of |j|" and an edge in the opposite direction are added.
  if (range.first == range.second)
    cont.insert(range.second, e);
}

void IRoadGraph::AddFakeEdges(geometry::PointWithAltitude const & junction,
                              vector<pair<Edge, geometry::PointWithAltitude>> const & vicinity)
{
  for (auto const & v : vicinity)
  {
    Edge const & ab = v.first;
    geometry::PointWithAltitude const p = v.second;

    vector<Edge> edges;
    SplitEdge(ab, p, edges);

    edges.push_back(Edge::MakeFake(junction, p));
    edges.push_back(Edge::MakeFake(p, junction));

    ForEachFakeEdge([&](Edge const & uv)
                    {
                      if (OnEdge(p, uv))
                        SplitEdge(uv, p, edges);
                    });

    for (auto const & uv : edges)
    {
      AddOutgoingFakeEdge(uv);
      AddIngoingFakeEdge(uv);
    }
  }
}

void IRoadGraph::AddOutgoingFakeEdge(Edge const & e)
{
  AddEdge(e.GetStartJunction(), e, m_fakeOutgoingEdges);
}

void IRoadGraph::AddIngoingFakeEdge(Edge const & e)
{
  AddEdge(e.GetEndJunction(), e, m_fakeIngoingEdges);
}

double IRoadGraph::GetSpeedKMpH(Edge const & edge, SpeedParams const & speedParams) const
{
  double const speedKMpH =
      (edge.IsFake() ? GetMaxSpeedKMpH()
                     : GetSpeedKMpH(edge.GetFeatureId(), speedParams));
  ASSERT_LESS_OR_EQUAL(speedKMpH, GetMaxSpeedKMpH(), ());
  return speedKMpH;
}

void IRoadGraph::GetEdgeTypes(Edge const & edge, feature::TypesHolder & types) const
{
  if (edge.IsFake())
    types = feature::TypesHolder(feature::GeomType::Line);
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
  UNREACHABLE();
}

IRoadGraph::RoadInfo MakeRoadInfoForTesting(bool bidirectional, double speedKMPH,
                                            initializer_list<m2::PointD> const & points)
{
  IRoadGraph::RoadInfo ri(bidirectional, speedKMPH, {});
  for (auto const & p : points)
    ri.m_junctions.emplace_back(geometry::MakePointWithAltitudeForTesting(p));

  return ri;
}
// RoadGraphBase ------------------------------------------------------------------
void RoadGraphBase::GetRouteEdges(EdgeVector & routeEdges) const
{
  NOTIMPLEMENTED()
}

void RoadGraphBase::GetRouteSegments(std::vector<Segment> &) const
{
  NOTIMPLEMENTED()
}
}  // namespace routing
