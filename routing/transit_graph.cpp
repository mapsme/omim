#include "routing/transit_graph.hpp"

#include "routing/index_graph.hpp"

#include "indexer/feature_altitude.hpp"

namespace routing
{
namespace
{
using namespace std;

Segment GetReverseSegment(Segment const & segment)
{
  return Segment(segment.GetMwmId(), segment.GetFeatureId(), segment.GetSegmentIdx(),
                 !segment.IsForward());
}

Junction const & GetStopJunction(map<transit::StopId, Junction> const & stopCoords,
                                 transit::StopId stopId)
{
  auto const it = stopCoords.find(stopId);
  CHECK(it != stopCoords.cend(), ("Stop", stopId, "does not exist."));
  return it->second;
}
}  // namespace

// static
uint32_t constexpr TransitGraph::kTransitFeatureId;

// static
bool TransitGraph::IsTransitFeature(uint32_t featureId) { return featureId == kTransitFeatureId; }

// static
bool TransitGraph::IsTransitSegment(Segment const & segment)
{
  return IsTransitFeature(segment.GetFeatureId());
}

TransitGraph::TransitGraph(NumMwmId numMwmId, shared_ptr<EdgeEstimator> estimator)
  : m_mwmId(numMwmId), m_estimator(estimator)
{
}

Junction const & TransitGraph::GetJunction(Segment const & segment, bool front) const
{
  CHECK(IsTransitSegment(segment), ("Nontransit segment passed to TransitGraph."));
  auto const & vertex = m_fake.GetVertex(segment);
  return front ? vertex.GetJunctionTo() : vertex.GetJunctionFrom();
}

RouteWeight TransitGraph::CalcSegmentWeight(Segment const & segment) const
{
  CHECK(IsTransitSegment(segment), ("Nontransit segment passed to TransitGraph."));
  if (IsGate(segment))
  {
    auto const weight = GetGate(segment).GetWeight();
    return RouteWeight(weight /* weight */, 0 /* nonPassThrougCross */, weight /* transitTime */);
  }

  if (IsEdge(segment))
  {
    auto const weight = GetEdge(segment).GetWeight();
    return RouteWeight(weight /* weight */, 0 /* nonPassThrougCross */, weight /* transitTime */);
  }

  return RouteWeight(
      m_estimator->CalcOffroadWeight(GetJunction(segment, false /* front */).GetPoint(),
                                     GetJunction(segment, true /* front */).GetPoint()));
}

RouteWeight TransitGraph::GetTransferPenalty(Segment const & from, Segment const & to) const
{
  // We need to wait transport and apply additional penalty only if we change to transit::Edge.
  if (!IsEdge(to))
    return GetAStarWeightZero<RouteWeight>();

  auto const & edgeTo = GetEdge(to);

  // We are changing to transfer and do not need to apply extra penalty here. We'll do it while
  // changing from transfer.
  if (edgeTo.GetTransfer())
    return GetAStarWeightZero<RouteWeight>();

  auto const lineIdTo = edgeTo.GetLineId();

  if (IsEdge(from) && GetEdge(from).GetLineId() == lineIdTo)
    return GetAStarWeightZero<RouteWeight>();

  // We need to apply extra penalty when:
  // 1. |from| is gate, |to| is edge
  // 2. |from| is transfer, |to| is edge
  // 3. |from| is edge, |to| is edge from another line directly connected to |from|.
  auto const it = m_transferPenalties.find(lineIdTo);
  CHECK(it != m_transferPenalties.cend(), ("Segment", to, "belongs to unknown line:", lineIdTo));
  return RouteWeight(it->second /* weight */, 0 /* nonPassThrougCross */,
                     it->second /* transitTime */);
}

void TransitGraph::GetTransitEdges(Segment const & segment, bool isOutgoing,
                                   vector<SegmentEdge> & edges) const
{
  CHECK(IsTransitSegment(segment), ("Nontransit segment passed to TransitGraph."));
  for (auto const & s : m_fake.GetEdges(segment, isOutgoing))
  {
    auto const & from = isOutgoing ? segment : s;
    auto const & to = isOutgoing ? s : segment;
    edges.emplace_back(s, CalcSegmentWeight(to) + GetTransferPenalty(from, to));
  }
}

set<Segment> const & TransitGraph::GetFake(Segment const & real) const
{
  return m_fake.GetFake(real);
}

bool TransitGraph::FindReal(Segment const & fake, Segment & real) const
{
  return m_fake.FindReal(fake, real);
}

void TransitGraph::Fill(TransitData const & transitData, GateEndings const & gateEndings)
{
  // Line has information about transit interval.
  // We assume arrival time has uniform distribution with min value |0| and max value |line.GetInterval()|.
  // Expected value of time to wait transport for particular line is |line.GetInterval() / 2|.
  for (auto const & line : transitData.m_lines)
    m_transferPenalties[line.GetId()] = line.GetInterval() / 2;

  map<transit::StopId, Junction> stopCoords;
  for (auto const & stop : transitData.m_stops)
    stopCoords[stop.GetId()] = Junction(stop.GetPoint(), feature::kDefaultAltitudeMeters);

  StopToSegmentsMap stopToBack;
  StopToSegmentsMap stopToFront;
  StopToSegmentsMap outgoing;
  StopToSegmentsMap ingoing;

  // It's important to add transit edges first to ensure fake segment id for particular edge is edge order
  // in mwm. We use edge fake segments in cross-mwm section and they should be stable.
  CHECK_EQUAL(m_fake.GetSize(), 0, ());
  for (size_t i = 0; i < transitData.m_edges.size(); ++i)
  {
    auto const & edge = transitData.m_edges[i];
    CHECK_NOT_EQUAL(edge.GetWeight(), transit::kInvalidWeight, ("Edge should have valid weight."));
    auto const edgeSegment = AddEdge(edge, stopCoords, stopToBack, stopToFront);
    // Check fake segment id is edge order in mwm.
    CHECK_EQUAL(edgeSegment.GetSegmentIdx(), i, ());
    outgoing[edge.GetStop1Id()].insert(edgeSegment);
    ingoing[edge.GetStop2Id()].insert(edgeSegment);
  }
  CHECK_EQUAL(m_fake.GetSize(), transitData.m_edges.size(), ());

  for (auto const & gate : transitData.m_gates)
  {
    CHECK_NOT_EQUAL(gate.GetWeight(), transit::kInvalidWeight, ("Gate should have valid weight."));

    // Gate ending may have empty projections vector. It means gate is not connected to roads
    auto const it = gateEndings.find(gate.GetOsmId());
    if (it != gateEndings.cend())
    {
      if (gate.GetEntrance())
        AddGate(gate, it->second, stopCoords, true /* isEnter */, stopToBack, stopToFront);
      if (gate.GetExit())
        AddGate(gate, it->second, stopCoords, false /* isEnter */, stopToBack, stopToFront);
    }
  }

  AddConnections(outgoing, stopToBack, stopToFront, true /* isOutgoing */);
  AddConnections(ingoing, stopToBack, stopToFront, false /* isOutgoing */);
}

bool TransitGraph::IsGate(Segment const & segment) const
{
  return m_segmentToGate.count(segment) > 0;
}

bool TransitGraph::IsEdge(Segment const & segment) const
{
  return m_segmentToEdge.count(segment) > 0;
}

transit::Gate const & TransitGraph::GetGate(Segment const & segment) const
{
  auto const it = m_segmentToGate.find(segment);
  CHECK(it != m_segmentToGate.cend(), ("Unknown transit segment."));
  return it->second;
}

transit::Edge const & TransitGraph::GetEdge(Segment const & segment) const
{
  auto const it = m_segmentToEdge.find(segment);
  CHECK(it != m_segmentToEdge.cend(), ("Unknown transit segment."));
  return it->second;
}

Segment TransitGraph::GetTransitSegment(uint32_t segmentIdx) const
{
  return Segment(m_mwmId, kTransitFeatureId, segmentIdx, false);
}

Segment TransitGraph::GetNewTransitSegment() const
{
  CHECK_LESS_OR_EQUAL(m_fake.GetSize(), numeric_limits<uint32_t>::max(), ());
  return GetTransitSegment(static_cast<uint32_t>(m_fake.GetSize()) /* segmentIdx */);
}

void TransitGraph::AddGate(transit::Gate const & gate, FakeEnding const & ending,
                           map<transit::StopId, Junction> const & stopCoords, bool isEnter,
                           StopToSegmentsMap & stopToBack, StopToSegmentsMap & stopToFront)
{
  Segment const dummy = Segment();
  for (auto const & projection : ending.m_projections)
  {
    // Add projection edges
    auto const projectionSegment = GetNewTransitSegment();
    FakeVertex projectionVertex(isEnter ? projection.m_junction : ending.m_originJunction,
                                isEnter ? ending.m_originJunction : projection.m_junction,
                                FakeVertex::Type::PureFake);
    m_fake.AddStandaloneVertex(projectionSegment, projectionVertex);

    // Add fake parts of real
    FakeVertex forwardPartOfReal(isEnter ? projection.m_segmentBack : projection.m_junction,
                                 isEnter ? projection.m_junction : projection.m_segmentFront,
                                 FakeVertex::Type::PartOfReal);
    auto const fakeForwardSegment = GetNewTransitSegment();
    m_fake.AddVertex(projectionSegment, fakeForwardSegment, forwardPartOfReal,
                     !isEnter /* isOutgoing */, true /* isPartOfReal */, projection.m_segment);

    if (!projection.m_isOneWay)
    {
      FakeVertex backwardPartOfReal(isEnter ? projection.m_segmentFront : projection.m_junction,
                                    isEnter ? projection.m_junction : projection.m_segmentBack,
                                    FakeVertex::Type::PartOfReal);
      auto const fakeBackwardSegment = GetNewTransitSegment();
      m_fake.AddVertex(projectionSegment, fakeBackwardSegment, backwardPartOfReal,
                       !isEnter /* isOutgoing */, true /* isPartOfReal */,
                       GetReverseSegment(projection.m_segment));
    }

    // Connect gate to stops
    for (auto const stopId : gate.GetStopIds())
    {
      auto const gateSegment = GetNewTransitSegment();
      auto const stopIt = stopCoords.find(stopId);
      CHECK(stopIt != stopCoords.end(), ("Stop", stopId, "does not exist."));
      FakeVertex gateVertex(isEnter ? ending.m_originJunction : stopIt->second,
                            isEnter ? stopIt->second : ending.m_originJunction,
                            FakeVertex::Type::PureFake);
      m_fake.AddVertex(projectionSegment, gateSegment, gateVertex, isEnter /* isOutgoing */,
                       false /* isPartOfReal */, dummy /* realSegment */);
      m_segmentToGate[gateSegment] = gate;
      if (isEnter)
        stopToFront[stopId].insert(gateSegment);
      else
        stopToBack[stopId].insert(gateSegment);
    }
  }
}

Segment TransitGraph::AddEdge(transit::Edge const & edge,
                              map<transit::StopId, Junction> const & stopCoords,
                              StopToSegmentsMap & stopToBack, StopToSegmentsMap & stopToFront)
{
  auto const edgeSegment = GetNewTransitSegment();
  auto const stopFromId = edge.GetStop1Id();
  auto const stopToId = edge.GetStop2Id();
  FakeVertex edgeVertex(GetStopJunction(stopCoords, stopFromId),
                        GetStopJunction(stopCoords, stopToId), FakeVertex::Type::PureFake);
  m_fake.AddStandaloneVertex(edgeSegment, edgeVertex);
  m_segmentToEdge[edgeSegment] = edge;
  stopToBack[stopFromId].insert(edgeSegment);
  stopToFront[stopToId].insert(edgeSegment);
  return edgeSegment;
}

void TransitGraph::AddConnections(StopToSegmentsMap const & connections,
                                  StopToSegmentsMap const & stopToBack,
                                  StopToSegmentsMap const & stopToFront, bool isOutgoing)
{
  for (auto const & connection : connections)
  {
    for (auto const & connectedSegment : connection.second)
    {
      auto const & adjacentSegments = isOutgoing ? stopToFront : stopToBack;
      auto const segmentsIt = adjacentSegments.find(connection.first);
      if (segmentsIt == adjacentSegments.cend())
        continue;
      for (auto const & segment : segmentsIt->second)
      {
        m_fake.AddConnection(isOutgoing ? segment : connectedSegment,
                             isOutgoing ? connectedSegment : segment);
      }
    }
  }
}

void MakeGateEndings(vector<transit::Gate> const & gates, NumMwmId mwmId,
                     IndexGraph & indexGraph, TransitGraph::GateEndings & gateEndings)
{
  for (auto const & gate : gates)
  {
    auto const & gateSegment = gate.GetBestPedestrianSegment();
    if (!gateSegment.IsValid())
      continue;

    Segment const real(mwmId, gateSegment.GetFeatureId(), gateSegment.GetSegmentIdx(),
                       gateSegment.GetForward());
    gateEndings.emplace(gate.GetOsmId(), MakeFakeEnding(real, gate.GetPoint(), indexGraph));
  }
}
}  // namespace routing
