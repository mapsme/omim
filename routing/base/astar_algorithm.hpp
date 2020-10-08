#pragma once

#include "routing/base/astar_graph.hpp"
#include "routing/base/astar_vertex_data.hpp"
#include "routing/base/astar_weight.hpp"
#include "routing/base/routing_result.hpp"

#include "base/assert.hpp"
#include "base/cancellable.hpp"
#include "base/fifo_cache.hpp"
#include "base/logging.hpp"

#include <algorithm>
#include <atomic>
#include <functional>
#include <iostream>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <type_traits>
#include <utility>
#include <vector>

#include "3party/skarupke/bytell_hash_map.hpp"

namespace routing
{
namespace astar
{
template <typename Vertex>
struct DefaultVisitor
{
  void operator()(Vertex const & /* from */, Vertex const & /* to */) const {};
};

template <typename Weight>
struct DefaultLengthChecker
{
  bool operator()(Weight const & /* weight */) const { return true; }
};
}  // namespace astar

template <typename Vertex, typename Edge, typename Weight>
class AStarAlgorithm
{
public:

  using Graph = AStarGraph<Vertex, Edge, Weight>;

  enum class Result
  {
    OK,
    NoPath,
    Cancelled
  };

  friend std::ostream & operator<<(std::ostream & os, Result const & result)
  {
    switch (result)
    {
    case Result::OK:
      os << "OK";
      break;
    case Result::NoPath:
      os << "NoPath";
      break;
    case Result::Cancelled:
      os << "Cancelled";
      break;
    }
    return os;
  }

  // |LengthChecker| callback used to check path length from start/finish to the edge (including the
  // edge itself) before adding the edge to AStar queue. Can be used to clip some path which does
  // not meet restrictions.
  template <typename Visitor = astar::DefaultVisitor<Vertex>,
            typename LengthChecker = astar::DefaultLengthChecker<Weight>>
  struct Params
  {
    Params(Graph & graph, Vertex const & startVertex, Vertex const & finalVertex,
           std::vector<Edge> const * prevRoute, base::Cancellable const & cancellable,
           Visitor && onVisitedVertexCallback = astar::DefaultVisitor<Vertex>(),
           LengthChecker && checkLengthCallback = astar::DefaultLengthChecker<Weight>())
        : m_graph(graph)
        , m_startVertex(startVertex)
        , m_finalVertex(finalVertex)
        , m_prevRoute(prevRoute)
        , m_cancellable(cancellable)
        , m_onVisitedVertexCallback(std::forward<Visitor>(onVisitedVertexCallback))
        , m_checkLengthCallback(std::forward<LengthChecker>(checkLengthCallback))
    {
    }

    Graph & m_graph;
    Weight const m_weightEpsilon = m_graph.GetAStarWeightEpsilon();
    Vertex const m_startVertex;
    // Used for FindPath, FindPathBidirectional.
    Vertex const m_finalVertex;
    // Used for AdjustRoute.
    std::vector<Edge> const * const m_prevRoute;
    base::Cancellable const & m_cancellable;
    Visitor m_onVisitedVertexCallback;
    LengthChecker const m_checkLengthCallback;
  };

  template <typename LengthChecker = astar::DefaultLengthChecker<Weight>>
  struct ParamsForTests
  {
    ParamsForTests(
        Graph & graph, Vertex const & startVertex, Vertex const & finalVertex,
        std::vector<Edge> const * prevRoute,
        LengthChecker && checkLengthCallback = astar::DefaultLengthChecker<Weight>())
      : m_graph(graph)
      , m_startVertex(startVertex)
      , m_finalVertex(finalVertex)
      , m_prevRoute(prevRoute)
      , m_checkLengthCallback(std::forward<LengthChecker>(checkLengthCallback))
    {
    }

    Graph & m_graph;
    Weight const m_weightEpsilon = m_graph.GetAStarWeightEpsilon();
    Vertex const m_startVertex;
    // Used for FindPath, FindPathBidirectional.
    Vertex const m_finalVertex;
    // Used for AdjustRoute.
    std::vector<Edge> const * const m_prevRoute;
    base::Cancellable const m_cancellable;
    astar::DefaultVisitor<Vertex> const m_onVisitedVertexCallback =
        astar::DefaultVisitor<Vertex>();
    LengthChecker const m_checkLengthCallback;
  };
  class Context final
  {
  public:
    Context(Graph & graph) : m_graph(graph)
    {
      m_graph.SetAStarParents(true /* forward */, m_parents);
    }

    ~Context()
    {
      m_graph.DropAStarParents();
    }

    void Clear()
    {
      m_distanceMap.clear();
      m_parents.clear();
    }

    bool HasDistance(Vertex const & vertex) const
    {
      return m_distanceMap.find(vertex) != m_distanceMap.cend();
    }

    Weight GetDistance(Vertex const & vertex) const
    {
      auto const & it = m_distanceMap.find(vertex);
      if (it == m_distanceMap.cend())
        return kInfiniteDistance;

      return it->second;
    }

    void SetDistance(Vertex const & vertex, Weight const & distance)
    {
      m_distanceMap[vertex] = distance;
    }

    void SetParent(Vertex const & parent, Vertex const & child)
    {
      m_parents[parent] = child;
    }

    bool HasParent(Vertex const & child) const
    {
      return m_parents.count(child) != 0;
    }

    Vertex const & GetParent(Vertex const & child) const
    {
      auto const it = m_parents.find(child);
      CHECK(it != m_parents.cend(), ("Can not find parent of child:", child));
      return it->second;
    }

    typename Graph::Parents & GetParents() { return m_parents; }

    void ReconstructPath(Vertex const & v, std::vector<Vertex> & path) const;

  private:
    Graph & m_graph;
    ska::bytell_hash_map<Vertex, Weight> m_distanceMap;
    typename Graph::Parents m_parents;
  };

  // VisitVertex returns true: wave will continue
  // VisitVertex returns false: wave will stop
  template <typename VisitVertex, typename AdjustEdgeWeight, typename FilterStates,
            typename ReducedToRealLength>
  void PropagateWave(Graph & graph, Vertex const & startVertex, VisitVertex && visitVertex,
                     AdjustEdgeWeight && adjustEdgeWeight, FilterStates && filterStates,
                     ReducedToRealLength && reducedToRealLength, Context & context) const;

  template <typename VisitVertex>
  void PropagateWave(Graph & graph, Vertex const & startVertex, VisitVertex && visitVertex,
                     Context & context) const;

  template <typename P>
  Result FindPath(P & params, RoutingResult<Vertex, Weight> & result) const;

  template <typename P>
  Result FindPathBidirectional(P & params, RoutingResult<Vertex, Weight> & result) const;
  template <typename P>
  Result FindPathBidirectionalOneThread(P & params, RoutingResult<Vertex, Weight> & result) const;

  // Adjust route to the previous one.
  // Expects |params.m_checkLengthCallback| to check wave propagation limit.
  template <typename P>
  typename AStarAlgorithm<Vertex, Edge, Weight>::Result AdjustRoute(P & params,
                                                                    RoutingResult<Vertex, Weight> & result) const;

private:
  // Periodicity of switching a wave of bidirectional algorithm.
  static uint32_t constexpr kQueueSwitchPeriod = 128;

  // Precision of comparison weights.
  static Weight constexpr kZeroDistance = GetAStarWeightZero<Weight>();
  static Weight constexpr kInfiniteDistance = GetAStarWeightMax<Weight>();

  class PeriodicPollCancellable final
  {
  public:
    PeriodicPollCancellable(base::Cancellable const & cancellable) : m_cancellable(cancellable) {}

    bool IsCancelled()
    {
      // Periodicity of checking is cancellable cancelled.
      uint32_t constexpr kPeriod = 128;
      return count++ % kPeriod == 0 && m_cancellable.IsCancelled();
    }

  private:
    base::Cancellable const & m_cancellable;
    uint32_t count = 0;
  };

  // State is what is going to be put in the priority queue. See the
  // comment for FindPath for more information.
  struct State
  {
    State(Vertex const & vertex, Weight const & distance, Weight const & heuristic)
        : vertex(vertex), distance(distance), heuristic(heuristic)
    {
    }
    State(Vertex const & vertex, Weight const & distance) : State(vertex, distance, Weight()) {}

    inline bool operator>(State const & rhs) const { return distance > rhs.distance; }

    Vertex vertex;
    Weight distance;
    Weight heuristic;
  };

  // BidirectionalStepContext keeps all the information that is needed to
  // search starting from one of the two directions. Its main
  // purpose is to make the code that changes directions more readable.
  struct BidirectionalStepContext
  {
    using Parents = typename Graph::Parents;

    BidirectionalStepContext(std::mutex & mtx, std::mutex & mtxGr, bool forward, Vertex const & startVertex,
                             Vertex const & finalVertex, Graph & graph)
      : mtx(mtx)
      , mtxGr(mtxGr)
      , forward(forward)
      , startVertex(startVertex)
      , finalVertex(finalVertex)
      , graph(graph)
//      , consistentHeuristics(300 /* capacity */, [this](Vertex const & v, Weight & w) {
//        this->ConsistentHeuristicImpl(v, w);
//      })
    {
      bestVertex = forward ? startVertex : finalVertex;
      pS = ConsistentHeuristic(bestVertex);
      graph.SetAStarParents(forward, parent);
    }

    ~BidirectionalStepContext()
    {
      graph.DropAStarParents();
    }

    Weight TopDistance() const
    {
      ASSERT(!queue.empty(), ());
      return bestDistance.at(queue.top().vertex);
    }

    // p_f(v) = 0.5*(π_f(v) - π_r(v))
    // p_r(v) = 0.5*(π_r(v) - π_f(v))
    // p_r(v) + p_f(v) = const. This condition is called consistence.
    //
    // Note. Adding constant terms to p_f(v) or p_r(v) does
    // not violate the consistence so a common choice is
    //     p_f(v) = 0.5*(π_f(v) - π_r(v)) + 0.5*π_r(t)
    //     p_r(v) = 0.5*(π_r(v) - π_f(v)) + 0.5*π_f(s)
    // which leads to p_f(t) = 0 and p_r(s) = 0.
    // However, with constants set to zero understanding
    // particular routes when debugging turned out to be easier.
    Weight ConsistentHeuristic(Vertex const & v) const
    {
      Weight piF;
      Weight piR;
      piF = graph.HeuristicCostEstimate(v, finalVertex, forward);
      piR = graph.HeuristicCostEstimate(v, startVertex, forward);

      if (forward)
      {
        /// @todo careful: with this "return" here and below in the Backward case
        /// the heuristic becomes inconsistent but still seems to work.
        /// return HeuristicCostEstimate(v, finalVertex);
        return 0.5 * (piF - piR);
      }
      else
      {
        // return HeuristicCostEstimate(v, startVertex);
        return 0.5 * (piR - piF);
      }
    }
//    void ConsistentHeuristicImpl(Vertex const & v, Weight & weight) const
//    {
//      auto const piF = graph.HeuristicCostEstimate(v, finalVertex);
//      auto const piR = graph.HeuristicCostEstimate(v, startVertex);
//      if (forward)
//      {
//        /// @todo careful: with this "return" here and below in the Backward case
//        /// the heuristic becomes inconsistent but still seems to work.
//        /// return HeuristicCostEstimate(v, finalVertex);
//        weight = 0.5 * (piF - piR);
//      }
//      else
//      {
//        // return HeuristicCostEstimate(v, startVertex);
//        weight = 0.5 * (piR - piF);
//      }
//    }
//
//    Weight ConsistentHeuristic(Vertex const & v)
//    {
//      return consistentHeuristics.GetValue(v);
//    }

    bool ExistsStateWithBetterDistance(State const & state, Weight const & eps = Weight(0.0)) const
    {
      auto const it = bestDistance.find(state.vertex);
      return it != bestDistance.end() && state.distance > it->second - eps;
    }

    void UpdateDistance(State const & state)
    {
      bestDistance.insert_or_assign(state.vertex, state.distance);
    }

    std::optional<Weight> GetDistance(Vertex const & vertex)
    {
      auto const it = bestDistance.find(vertex);
      return it != bestDistance.cend() ? std::optional<Weight>(it->second) : std::nullopt;
    }

    void UpdateParent(Vertex const & to, Vertex const & from)
    {
      parent.insert_or_assign(to, from);
    }

    void GetAdjacencyList(State const & state, std::vector<Edge> & adj)
    {
      auto const realDistance = state.distance + pS - state.heuristic;
      astar::VertexData const data(state.vertex, realDistance);

//      std::lock_guard lock(mtxGr);
      if (forward)
        graph.GetOutgoingEdgesList(data, adj);
      else
        graph.GetIngoingEdgesList(data, adj);
    }

    Parents & GetParents() { return parent; }

    bool ExistsStateWithBetterDistanceLock(State const & state, Weight const & eps = Weight(0.0))
    {
      std::lock_guard lock(mtx);
      return ExistsStateWithBetterDistance(state, eps);
    }

    void UpdateDistanceLock(State const & state)
    {
      std::lock_guard lock(mtx);
      UpdateDistance(state);
    }

    std::optional<Weight> GetDistanceLock(Vertex const & vertex)
    {
      std::lock_guard lock(mtx);
      return GetDistance(vertex);
    }

//    std::atomic<bool> fullProtection;
    std::mutex & mtx;
    std::mutex & mtxGr;
    bool const forward;
    Vertex const & startVertex;
    Vertex const & finalVertex;
    Graph & graph;

    std::priority_queue<State, std::vector<State>, std::greater<State>> queue;
    ska::bytell_hash_map<Vertex, Weight> bestDistance;
//    FifoCache<Vertex, Weight> consistentHeuristics;
    Parents parent;
    Vertex bestVertex;

    Weight pS;
  };

  static void ReconstructPath(Vertex const & v,
                              typename BidirectionalStepContext::Parents const & parent,
                              std::vector<Vertex> & path);
  static void ReconstructPathBidirectional(
      Vertex const & v, Vertex const & w,
      typename BidirectionalStepContext::Parents const & parentV,
      typename BidirectionalStepContext::Parents const & parentW, std::vector<Vertex> & path);
};

template <typename Vertex, typename Edge, typename Weight>
constexpr Weight AStarAlgorithm<Vertex, Edge, Weight>::kInfiniteDistance;
template <typename Vertex, typename Edge, typename Weight>
constexpr Weight AStarAlgorithm<Vertex, Edge, Weight>::kZeroDistance;

template <typename Vertex, typename Edge, typename Weight>
template <typename VisitVertex, typename AdjustEdgeWeight, typename FilterStates, typename ReducedToFullLength>
void AStarAlgorithm<Vertex, Edge, Weight>::PropagateWave(
    Graph & graph, Vertex const & startVertex,
    VisitVertex && visitVertex,
    AdjustEdgeWeight && adjustEdgeWeight,
    FilterStates && filterStates,
    ReducedToFullLength && reducedToFullLength,
    AStarAlgorithm<Vertex, Edge, Weight>::Context & context) const
{
  auto const epsilon = graph.GetAStarWeightEpsilon();

  context.Clear();

  std::priority_queue<State, std::vector<State>, std::greater<State>> queue;

  context.SetDistance(startVertex, kZeroDistance);
  queue.push(State(startVertex, kZeroDistance));

  std::vector<Edge> adj;

  while (!queue.empty())
  {
    State const stateV = queue.top();
    queue.pop();

    if (stateV.distance > context.GetDistance(stateV.vertex))
      continue;

    if (!visitVertex(stateV.vertex))
      return;

    astar::VertexData const vertexData(stateV.vertex, reducedToFullLength(stateV));
    graph.GetOutgoingEdgesList(vertexData, adj);
    for (auto const & edge : adj)
    {
      State stateW(edge.GetTarget(), kZeroDistance);
      if (stateV.vertex == stateW.vertex)
        continue;

      auto const edgeWeight = adjustEdgeWeight(stateV.vertex, edge);
      auto const newReducedDist = stateV.distance + edgeWeight;

      if (newReducedDist >= context.GetDistance(stateW.vertex) - epsilon)
        continue;

      stateW.distance = newReducedDist;

      if (!filterStates(stateW))
        continue;

      context.SetDistance(stateW.vertex, newReducedDist);
      context.SetParent(stateW.vertex, stateV.vertex);
      queue.push(stateW);
    }
  }
}

template <typename Vertex, typename Edge, typename Weight>
template <typename VisitVertex>
void AStarAlgorithm<Vertex, Edge, Weight>::PropagateWave(
    Graph & graph, Vertex const & startVertex, VisitVertex && visitVertex,
    AStarAlgorithm<Vertex, Edge, Weight>::Context & context) const
{
  auto const adjustEdgeWeight = [](Vertex const & /* vertex */, Edge const & edge) {
    return edge.GetWeight();
  };
  auto const filterStates = [](State const & /* state */) { return true; };
  auto const reducedToRealLength = [](State const & state) { return state.distance; };
  PropagateWave(graph, startVertex, visitVertex, adjustEdgeWeight, filterStates,
                reducedToRealLength, context);
}

// This implementation is based on the view that the A* algorithm
// is equivalent to Dijkstra's algorithm that is run on a reweighted
// version of the graph. If an edge (v, w) has length l(v, w), its reduced
// cost is l_r(v, w) = l(v, w) + pi(w) - pi(v), where pi() is any function
// that ensures l_r(v, w) >= 0 for every edge. We set pi() to calculate
// the shortest possible distance to a goal node, and this is a common
// heuristic that people use in A*.
// Refer to these papers for more information:
// http://research.microsoft.com/pubs/154937/soda05.pdf
// http://www.cs.princeton.edu/courses/archive/spr06/cos423/Handouts/EPP%20shortest%20path%20algorithms.pdf

template <typename Vertex, typename Edge, typename Weight>
template <typename P>
typename AStarAlgorithm<Vertex, Edge, Weight>::Result
AStarAlgorithm<Vertex, Edge, Weight>::FindPath(P & params, RoutingResult<Vertex, Weight> & result) const
{
  auto const epsilon = params.m_weightEpsilon;

  result.Clear();

  auto & graph = params.m_graph;
  auto const & finalVertex = params.m_finalVertex;
  auto const & startVertex = params.m_startVertex;

  Context context(graph);
  PeriodicPollCancellable periodicCancellable(params.m_cancellable);
  Result resultCode = Result::NoPath;

  auto const heuristicDiff = [&](Vertex const & vertexFrom, Vertex const & vertexTo) {
    return graph.HeuristicCostEstimate(vertexFrom, finalVertex, true) -
           graph.HeuristicCostEstimate(vertexTo, finalVertex, true);
  };

  auto const fullToReducedLength = [&](Vertex const & vertexFrom, Vertex const & vertexTo,
                                       Weight const length) {
    return length - heuristicDiff(vertexFrom, vertexTo);
  };

  auto const reducedToFullLength = [&](Vertex const & vertexFrom, Vertex const & vertexTo,
                                       Weight const reducedLength) {
    return reducedLength + heuristicDiff(vertexFrom, vertexTo);
  };

  auto visitVertex = [&](Vertex const & vertex) {
    if (periodicCancellable.IsCancelled())
    {
      resultCode = Result::Cancelled;
      return false;
    }

    params.m_onVisitedVertexCallback(vertex, finalVertex);

    if (vertex == finalVertex)
    {
      resultCode = Result::OK;
      return false;
    }

    return true;
  };

  auto const adjustEdgeWeight = [&](Vertex const & vertexV, Edge const & edge) {
    auto const reducedWeight = fullToReducedLength(vertexV, edge.GetTarget(), edge.GetWeight());

    CHECK_GREATER_OR_EQUAL(reducedWeight, -epsilon, ("Invariant violated."));

    return std::max(reducedWeight, kZeroDistance);
  };

  auto const reducedToRealLength = [&](State const & state) {
    return reducedToFullLength(startVertex, state.vertex, state.distance);
  };

  auto const filterStates = [&](State const & state) {
    return params.m_checkLengthCallback(reducedToRealLength(state));
  };

  PropagateWave(graph, startVertex, visitVertex, adjustEdgeWeight, filterStates,
                reducedToRealLength, context);

  if (resultCode == Result::OK)
  {
    context.ReconstructPath(finalVertex, result.m_path);
    result.m_distance =
        reducedToFullLength(startVertex, finalVertex, context.GetDistance(finalVertex));

    if (!params.m_checkLengthCallback(result.m_distance))
      resultCode = Result::NoPath;
  }

  return resultCode;
}

template <typename Vertex, typename Edge, typename Weight>
template <typename P>
typename AStarAlgorithm<Vertex, Edge, Weight>::Result
AStarAlgorithm<Vertex, Edge, Weight>::FindPathBidirectional(P & params,
                                                            RoutingResult<Vertex, Weight> & result) const
{
  auto const epsilon = params.m_weightEpsilon;
  auto & graph = params.m_graph;
  auto const & finalVertex = params.m_finalVertex;
  auto const & startVertex = params.m_startVertex;

  std::mutex mtx;
  std::mutex mtxGr;
  BidirectionalStepContext forward(mtx, mtxGr, true /* forward */, startVertex, finalVertex, graph);
  BidirectionalStepContext backward(mtx, mtxGr, false /* forward */, startVertex, finalVertex, graph);

  auto & forwardParents = forward.GetParents();
  auto & backwardParents = backward.GetParents();

  bool foundAnyPath = false;
  auto bestPathReducedLength = kZeroDistance;
  auto bestPathRealLength = kZeroDistance;

  forward.UpdateDistance(State(startVertex, kZeroDistance));
  forward.queue.push(State(startVertex, kZeroDistance, forward.ConsistentHeuristic(startVertex)));

  backward.UpdateDistance(State(finalVertex, kZeroDistance));
  backward.queue.push(State(finalVertex, kZeroDistance, backward.ConsistentHeuristic(finalVertex)));

  auto wave = [&epsilon](BidirectionalStepContext & context, std::atomic<bool> & queueIsEmpty,
                 BidirectionalStepContext & oppositeContext, std::atomic<bool> & oppositeQueueIsEmpty)
  {
    LOG(LINFO, ("---FindPath-------wave---------------------------", context.forward ? "forward" : "backward"));
    size_t i = 0;
    std::vector<Edge> adj;
    while (!context.queue.empty() /* && !oppositeQueueIsEmpty.load() */)
    {
      // TODO Take care exception top/pop problem.
      State const stateV = context.queue.top();
      context.queue.pop();

      if (context.ExistsStateWithBetterDistanceLock(stateV))
        continue;

//      params.m_onVisitedVertexCallback(stateV.vertex, context.forward ? context.finalVertex : context.startVertex);
      // @TODO Think about concurrent access to graph.
      context.GetAdjacencyList(stateV, adj);
      auto const & pV = stateV.heuristic;
      for (auto const & edge : adj)
      {
        State stateW(edge.GetTarget(), kZeroDistance);

        if (stateV.vertex == stateW.vertex)
          continue;

        auto const weight = edge.GetWeight();
        auto const pW = context.ConsistentHeuristic(stateW.vertex);
        auto const reducedWeight = weight + pW - pV;

        CHECK_GREATER_OR_EQUAL(
            reducedWeight, -epsilon,
            ("Invariant violated for:", "v =", stateV.vertex, "w =", stateW.vertex));

        stateW.distance = stateV.distance + std::max(reducedWeight, kZeroDistance);

//        auto const fullLength = weight + stateV.distance + context.pS - pV;

//        if (!params.m_checkLengthCallback(fullLength))
//          continue;

        if (context.ExistsStateWithBetterDistanceLock(stateW, epsilon))
          continue;

        stateW.heuristic = pW;
        context.UpdateDistanceLock(stateW);
        context.UpdateParent(stateW.vertex, stateV.vertex);
        ++i;
        if (oppositeContext.GetDistanceLock(stateW.vertex)) // i == 896199
        {
          LOG(LINFO, ("---FindPath-------wave end---------------------------", context.forward ? "forward" : "backward", stateW.vertex, i));
          return;  // The waves are intersected.
        }

        if (stateW.vertex != (context.forward ? context.finalVertex : context.startVertex))
          context.queue.push(stateW);
      }
    }

    if (context.queue.empty())
      queueIsEmpty.store(true);
    LOG(LINFO, ("----FindPath------wave end (empty)---------------------------", context.forward ? "forward" : "backward"));
  };

//  std::atomic<bool> foundAnyPathF;
  std::atomic<bool> fwdIsEmpty;
  std::atomic<bool> bwdIsEmpty;

  PeriodicPollCancellable periodicCancellable(params.m_cancellable);

  // Starting a thread.
  {
    base::ScopedTimerWithLog timer("Wave");
    auto backwardWave = std::async(std::launch::async, wave, std::ref(backward),
                                   std::ref(bwdIsEmpty), std::ref(forward), std::ref(fwdIsEmpty));
    wave(forward, fwdIsEmpty, backward, bwdIsEmpty);
    backwardWave.get();
  }
  LOG(LINFO, ("-------FindPath-------Finished-----------------"));

  // To use the search code both for backward and forward directions
  // we keep the pointers to everything related to the search in the
  // 'current' and 'next' directions. Swapping these pointers indicates
  // changing the end we are searching from.
  BidirectionalStepContext * cur = &forward;
  BidirectionalStepContext * nxt = &backward;

  auto const getResult = [&]() {
    if (!params.m_checkLengthCallback(bestPathRealLength))
      return Result::NoPath;

    ReconstructPathBidirectional(cur->bestVertex, nxt->bestVertex, cur->parent, nxt->parent,
                                 result.m_path);
    result.m_distance = bestPathRealLength;
    CHECK(!result.m_path.empty(), ());
    if (!cur->forward)
      reverse(result.m_path.begin(), result.m_path.end());

    return Result::OK;
  };

  std::vector<Edge> adj;
  uint32_t steps = 0;

  // Waves have been intersected. The routing is finished in one thread.
  while (!cur->queue.empty() && !nxt->queue.empty())
  {
    ++steps;

    if (periodicCancellable.IsCancelled())
      return Result::Cancelled;

    if (steps % kQueueSwitchPeriod == 0)
      std::swap(cur, nxt);

    if (foundAnyPath)
    {
      auto const curTop = cur->TopDistance();
      auto const nxtTop = nxt->TopDistance();

      // The intuition behind this is that we cannot obtain a path shorter
      // than the left side of the inequality because that is how any path we find
      // will look like (see comment for curPathReducedLength below).
      // We do not yet have the proof that we will not miss a good path by doing so.

      // The shortest reduced path corresponds to the shortest real path
      // because the heuristic we use are consistent.
      // It would be a mistake to make a decision based on real path lengths because
      // several top states in a priority queue may have equal reduced path lengths and
      // different real path lengths.

      if (curTop + nxtTop >= bestPathReducedLength - epsilon)
        return getResult();
    }

    State const stateV = cur->queue.top();
    cur->queue.pop();

    if (cur->ExistsStateWithBetterDistance(stateV))
      continue;

    params.m_onVisitedVertexCallback(stateV.vertex,
                                     cur->forward ? cur->finalVertex : cur->startVertex);

    cur->GetAdjacencyList(stateV, adj);
    auto const & pV = stateV.heuristic;
    for (auto const & edge : adj)
    {
      State stateW(edge.GetTarget(), kZeroDistance);

      if (stateV.vertex == stateW.vertex)
        continue;

      auto const weight = edge.GetWeight();
      auto const pW = cur->ConsistentHeuristic(stateW.vertex);
      auto const reducedWeight = weight + pW - pV;

      CHECK_GREATER_OR_EQUAL(reducedWeight, -epsilon,
                             ("Invariant violated for:", "v =", stateV.vertex, "w =", stateW.vertex));

      stateW.distance = stateV.distance + std::max(reducedWeight, kZeroDistance);

      auto const fullLength = weight + stateV.distance + cur->pS - pV;
      if (!params.m_checkLengthCallback(fullLength))
        continue;

      if (cur->ExistsStateWithBetterDistance(stateW, epsilon))
        continue;

      stateW.heuristic = pW;
      cur->UpdateDistance(stateW);
      cur->UpdateParent(stateW.vertex, stateV.vertex);

      if (auto op = nxt->GetDistance(stateW.vertex); op)
      {
        auto const & distW = *op;
        // Reduced length that the path we've just found has in the original graph:
        // find the reduced length of the path's parts in the reduced forward and backward graphs.
        auto const curPathReducedLength = stateW.distance + distW;
        // No epsilon here: it is ok to overshoot slightly.
        if ((!foundAnyPath || bestPathReducedLength > curPathReducedLength) &&
            graph.AreWavesConnectible(forwardParents, stateW.vertex, backwardParents))
        {
          bestPathReducedLength = curPathReducedLength;

          bestPathRealLength = stateV.distance + weight + distW;
          bestPathRealLength += cur->pS - pV;
          bestPathRealLength += nxt->pS - nxt->ConsistentHeuristic(stateW.vertex);

          foundAnyPath = true;
          cur->bestVertex = stateV.vertex;
          nxt->bestVertex = stateW.vertex;
        }
      }

      if (stateW.vertex != (cur->forward ? cur->finalVertex : cur->startVertex))
        cur->queue.push(stateW);
    }
  }

  if (foundAnyPath)
    return getResult();

  return Result::NoPath;
}

template <typename Vertex, typename Edge, typename Weight>
template <typename P>
typename AStarAlgorithm<Vertex, Edge, Weight>::Result
AStarAlgorithm<Vertex, Edge, Weight>::FindPathBidirectionalOneThread(P & params,
                                                            RoutingResult<Vertex, Weight> & result) const
{
  auto const epsilon = params.m_weightEpsilon;
  auto & graph = params.m_graph;
  auto const & finalVertex = params.m_finalVertex;
  auto const & startVertex = params.m_startVertex;

  std::mutex mtx;
  BidirectionalStepContext forward(mtx, true /* forward */, startVertex, finalVertex, graph);
  BidirectionalStepContext backward(mtx, false /* forward */, startVertex, finalVertex, graph);

  auto & forwardParents = forward.GetParents();
  auto & backwardParents = backward.GetParents();

  bool foundAnyPath = false;
  auto bestPathReducedLength = kZeroDistance;
  auto bestPathRealLength = kZeroDistance;

  forward.UpdateDistance(State(startVertex, kZeroDistance));
  forward.queue.push(State(startVertex, kZeroDistance, forward.ConsistentHeuristic(startVertex)));

  backward.UpdateDistance(State(finalVertex, kZeroDistance));
  backward.queue.push(State(finalVertex, kZeroDistance, backward.ConsistentHeuristic(finalVertex)));

  // To use the search code both for backward and forward directions
  // we keep the pointers to everything related to the search in the
  // 'current' and 'next' directions. Swapping these pointers indicates
  // changing the end we are searching from.
  BidirectionalStepContext * cur = &forward;
  BidirectionalStepContext * nxt = &backward;

  auto const getResult = [&]() {
    if (!params.m_checkLengthCallback(bestPathRealLength))
      return Result::NoPath;

    ReconstructPathBidirectional(cur->bestVertex, nxt->bestVertex, cur->parent, nxt->parent,
                                 result.m_path);
    result.m_distance = bestPathRealLength;
    CHECK(!result.m_path.empty(), ());
    if (!cur->forward)
      reverse(result.m_path.begin(), result.m_path.end());

    return Result::OK;
  };

  std::vector<Edge> adj;

  // It is not necessary to check emptiness for both queues here
  // because if we have not found a path by the time one of the
  // queues is exhausted, we never will.
  uint32_t steps = 0;
  PeriodicPollCancellable periodicCancellable(params.m_cancellable);

  while (!cur->queue.empty() && !nxt->queue.empty())
  {
    ++steps;

    if (periodicCancellable.IsCancelled())
      return Result::Cancelled;

    if (steps % kQueueSwitchPeriod == 0)
      std::swap(cur, nxt);

    if (foundAnyPath)
    {
      auto const curTop = cur->TopDistance();
      auto const nxtTop = nxt->TopDistance();

      // The intuition behind this is that we cannot obtain a path shorter
      // than the left side of the inequality because that is how any path we find
      // will look like (see comment for curPathReducedLength below).
      // We do not yet have the proof that we will not miss a good path by doing so.

      // The shortest reduced path corresponds to the shortest real path
      // because the heuristic we use are consistent.
      // It would be a mistake to make a decision based on real path lengths because
      // several top states in a priority queue may have equal reduced path lengths and
      // different real path lengths.

      if (curTop + nxtTop >= bestPathReducedLength - epsilon)
        return getResult();
    }

    State const stateV = cur->queue.top();
    cur->queue.pop();

    if (cur->ExistsStateWithBetterDistance(stateV))
      continue;

    params.m_onVisitedVertexCallback(stateV.vertex,
                                     cur->forward ? cur->finalVertex : cur->startVertex);

    cur->GetAdjacencyList(stateV, adj);
    auto const & pV = stateV.heuristic;
    for (auto const & edge : adj)
    {
      State stateW(edge.GetTarget(), kZeroDistance);

      if (stateV.vertex == stateW.vertex)
        continue;

      auto const weight = edge.GetWeight();
      auto const pW = cur->ConsistentHeuristic(stateW.vertex);
      auto const reducedWeight = weight + pW - pV;

      CHECK_GREATER_OR_EQUAL(reducedWeight, -epsilon,
                             ("Invariant violated for:", "v =", stateV.vertex, "w =", stateW.vertex));

      stateW.distance = stateV.distance + std::max(reducedWeight, kZeroDistance);

      auto const fullLength = weight + stateV.distance + cur->pS - pV;
      if (!params.m_checkLengthCallback(fullLength))
        continue;

      if (cur->ExistsStateWithBetterDistance(stateW, epsilon))
        continue;

      stateW.heuristic = pW;
      cur->UpdateDistance(stateW);
      cur->UpdateParent(stateW.vertex, stateV.vertex);

      if (auto op = nxt->GetDistance(stateW.vertex); op)
      {
        auto const & distW = *op;
        // Reduced length that the path we've just found has in the original graph:
        // find the reduced length of the path's parts in the reduced forward and backward graphs.
        auto const curPathReducedLength = stateW.distance + distW;
        // No epsilon here: it is ok to overshoot slightly.
        if ((!foundAnyPath || bestPathReducedLength > curPathReducedLength) &&
            graph.AreWavesConnectible(forwardParents, stateW.vertex, backwardParents))
        {
          bestPathReducedLength = curPathReducedLength;

          bestPathRealLength = stateV.distance + weight + distW;
          bestPathRealLength += cur->pS - pV;
          bestPathRealLength += nxt->pS - nxt->ConsistentHeuristic(stateW.vertex);

          foundAnyPath = true;
          cur->bestVertex = stateV.vertex;
          nxt->bestVertex = stateW.vertex;
        }
      }

      if (stateW.vertex != (cur->forward ? cur->finalVertex : cur->startVertex))
        cur->queue.push(stateW);
    }
  }

  if (foundAnyPath)
    return getResult();

  return Result::NoPath;
}


template <typename Vertex, typename Edge, typename Weight>
template <typename P>
typename AStarAlgorithm<Vertex, Edge, Weight>::Result
AStarAlgorithm<Vertex, Edge, Weight>::AdjustRoute(P & params,
                                                  RoutingResult<Vertex, Weight> & result) const
{
  CHECK(params.m_prevRoute, ());
  auto & graph = params.m_graph;
  auto const & startVertex = params.m_startVertex;
  auto const & prevRoute = *params.m_prevRoute;

  CHECK(!prevRoute.empty(), ());

  static_assert(!std::is_same<decltype(params.m_checkLengthCallback),
                              astar::DefaultLengthChecker<Weight>>::value,
                "CheckLengthCallback expected to be set to limit wave propagation.");

  result.Clear();

  bool wasCancelled = false;
  auto minDistance = kInfiniteDistance;
  Vertex returnVertex;

  std::map<Vertex, Weight> remainingDistances;
  auto remainingDistance = kZeroDistance;

  for (auto it = prevRoute.crbegin(); it != prevRoute.crend(); ++it)
  {
    remainingDistances[it->GetTarget()] = remainingDistance;
    remainingDistance += it->GetWeight();
  }

  Context context(graph);
  PeriodicPollCancellable periodicCancellable(params.m_cancellable);

  auto visitVertex = [&](Vertex const & vertex) {

    if (periodicCancellable.IsCancelled())
    {
      wasCancelled = true;
      return false;
    }

    params.m_onVisitedVertexCallback(startVertex, vertex);

    auto it = remainingDistances.find(vertex);
    if (it != remainingDistances.cend())
    {
      auto const fullDistance = context.GetDistance(vertex) + it->second;
      if (fullDistance < minDistance)
      {
        minDistance = fullDistance;
        returnVertex = vertex;
      }
    }

    return true;
  };

  auto const adjustEdgeWeight = [](Vertex const & /* vertex */, Edge const & edge) {
    return edge.GetWeight();
  };

  auto const filterStates = [&](State const & state) {
    return params.m_checkLengthCallback(state.distance);
  };

  auto const reducedToRealLength = [&](State const & state) { return state.distance; };

  PropagateWave(graph, startVertex, visitVertex, adjustEdgeWeight, filterStates,
                reducedToRealLength, context);
  if (wasCancelled)
    return Result::Cancelled;

  if (minDistance == kInfiniteDistance)
    return Result::NoPath;

  context.ReconstructPath(returnVertex, result.m_path);

  // Append remaining route.
  bool found = false;
  for (size_t i = 0; i < prevRoute.size(); ++i)
  {
    if (prevRoute[i].GetTarget() == returnVertex)
    {
      for (size_t j = i + 1; j < prevRoute.size(); ++j)
        result.m_path.push_back(prevRoute[j].GetTarget());

      found = true;
      break;
    }
  }

  CHECK(found, ("Can't find", returnVertex, ", prev:", prevRoute.size(),
                ", adjust:", result.m_path.size()));

  auto const & it = remainingDistances.find(returnVertex);
  CHECK(it != remainingDistances.end(), ());
  result.m_distance = context.GetDistance(returnVertex) + it->second;
  return Result::OK;
}

// static
template <typename Vertex, typename Edge, typename Weight>
void AStarAlgorithm<Vertex, Edge, Weight>::ReconstructPath(
    Vertex const & v, typename BidirectionalStepContext::Parents const & parent,
    std::vector<Vertex> & path)
{
  path.clear();
  Vertex cur = v;
  while (true)
  {
    path.push_back(cur);
    auto const it = parent.find(cur);
    if (it == parent.end())
      break;
    cur = it->second;
  }

  std::reverse(path.begin(), path.end());
}

// static
template <typename Vertex, typename Edge, typename Weight>
void AStarAlgorithm<Vertex, Edge, Weight>::ReconstructPathBidirectional(
    Vertex const & v, Vertex const & w, typename BidirectionalStepContext::Parents const & parentV,
    typename BidirectionalStepContext::Parents const & parentW, std::vector<Vertex> & path)
{
  std::vector<Vertex> pathV;
  ReconstructPath(v, parentV, pathV);
  std::vector<Vertex> pathW;
  ReconstructPath(w, parentW, pathW);
  path.clear();
  path.reserve(pathV.size() + pathW.size());
  path.insert(path.end(), pathV.begin(), pathV.end());
  path.insert(path.end(), pathW.rbegin(), pathW.rend());
}

template <typename Vertex, typename Edge, typename Weight>
void
AStarAlgorithm<Vertex, Edge, Weight>::Context::ReconstructPath(Vertex const & v,
                                                               std::vector<Vertex> & path) const
{
  AStarAlgorithm<Vertex, Edge, Weight>::ReconstructPath(v, m_parents, path);
}
}  // namespace routing
