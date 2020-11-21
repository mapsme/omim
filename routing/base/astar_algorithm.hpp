#pragma once

#include "routing/base/astar_graph.hpp"
#include "routing/base/astar_vertex_data.hpp"
#include "routing/base/astar_weight.hpp"
#include "routing/base/routing_result.hpp"

#include "base/assert.hpp"
#include "base/cancellable.hpp"
#include "base/logging.hpp"
#include "base/optional_lock_guard.hpp"
#include "base/thread.hpp"

#include <algorithm>
#include <atomic>
#include <functional>
#include <iostream>
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
  void operator()(Vertex const & /* from */, Vertex const & /* to */, bool /* isOutgoing */,
                  std::optional<std::mutex> & /* m */) const {};
};

template <typename Weight>
struct DefaultLengthChecker
{
  bool operator()(Weight const & /* weight */, bool /* isOutgoing */) const { return true; }
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

  /// \brief Finds path on |params.m_graph| using bidirectional A* algorithm.
  /// \note Two thread version is used when the version is set in |params.m_graph|.
  /// If |params.m_graph.IsTwoThreadsReady()| returns false, one thread version is used.
  /// It's worth using one thread version if there's only one core available.
  /// if |params.m_graph.IsTwoThreadsReady()| returns true two thread version is used.
  /// If the decision is made to use two thread version it should be taken into account:
  /// - |isOutgoing| flag in each method specified which thread calls the method
  /// - All callbacks which are called from |wave| lambda such as |params.m_onVisitedVertexCallback|
  ///   or |params.m_checkLengthCallback| should be ready for calling from two threads.
  template <typename P>
  Result FindPathBidirectional(P & params, RoutingResult<Vertex, Weight> & result) const;

  // Adjust route to the previous one.
  // Expects |params.m_checkLengthCallback| to check wave propagation limit.
  template <typename P>
  typename AStarAlgorithm<Vertex, Edge, Weight>::Result AdjustRoute(
      P & params, RoutingResult<Vertex, Weight> & result) const;

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

    BidirectionalStepContext(std::optional<std::mutex> & m, bool forward,
                             Vertex const & startVertex, Vertex const & finalVertex, Graph & graph)
      : mtx(m), forward(forward), startVertex(startVertex), finalVertex(finalVertex), graph(graph)
    {
      bestVertex = forward ? startVertex : finalVertex;
      pS = ConsistentHeuristic(bestVertex);
      graph.SetAStarParents(forward, parent);
    }

    ~BidirectionalStepContext()
    {
      graph.DropAStarParents();
    }

    void Init(Vertex const & endVertex)
    {
      UpdateDistance(State(endVertex, kZeroDistance));
      queue.push(State(endVertex, kZeroDistance, ConsistentHeuristic(endVertex)));
    }

    Weight TopDistance() const
    {
      ASSERT(stateV || !queue.empty(),
             ("Either stateV should have value or queue should have value(s)."));
      return bestDistance.at(stateV ? stateV->vertex : queue.top().vertex);
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
      bool const isTwoThreads = IsTwoThreadsReady();
      auto const piF = graph.HeuristicCostEstimate(v, finalVertex, isTwoThreads ?  forward : true);
      auto const piR = graph.HeuristicCostEstimate(v, startVertex, isTwoThreads ?  forward : true);
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

    bool ExistsStateWithBetterDistance(State const & state, Weight const & eps = Weight(0.0)) const
    {
      base::OptionalLockGuard guard(mtx);
      auto const it = bestDistance.find(state.vertex);
      return it != bestDistance.end() && state.distance > it->second - eps;
    }

    void UpdateDistance(State const & state)
    {
      base::OptionalLockGuard guard(mtx);
      bestDistance.insert_or_assign(state.vertex, state.distance);
    }

    std::optional<Weight> GetDistance(Vertex const & vertex) const
    {
      base::OptionalLockGuard guard(mtx);
      auto const it = bestDistance.find(vertex);
      return it != bestDistance.cend() ? std::optional<Weight>(it->second) : std::nullopt;
    }

    void UpdateParent(Vertex const & to, Vertex const & from)
    {
      parent.insert_or_assign(to, from);
    }

    void Update(State const & stateW, State const & stateV)
    {
      UpdateDistance(stateW);
      UpdateParent(stateW.vertex, stateV.vertex);
    }

    void UpdateAndPushIfNotEnd(State const & stateW, State const & stateV)
    {
      Update(stateW, stateV);
      if (stateW.vertex != GetEndVertex())
        queue.push(stateW);
    }

    void FillAdjacencyList(State const & state)
    {
      auto const realDistance = state.distance + pS - state.heuristic;
      astar::VertexData const data(state.vertex, realDistance);
      if (forward)
        graph.GetOutgoingEdgesList(data, adj);
      else
        graph.GetIngoingEdgesList(data, adj);
    }

    State TopAndPopState()
    {
      State state = stateV ? *stateV : queue.top();
      if (stateV)
        stateV = std::nullopt;
      else
        queue.pop();
      return state;
    }

    Parents & GetParents() { return parent; }

    Vertex const & GetEndVertex() const { return forward ? finalVertex : startVertex; }

    bool IsNextVertex() const {return !queue.empty() || stateV; }

    bool IsTwoThreadsReady() const { return mtx.has_value(); }

    std::optional<std::mutex> & mtx;
    bool const forward;
    Vertex const & startVertex;
    Vertex const & finalVertex;
    Graph & graph;

    std::priority_queue<State, std::vector<State>, std::greater<State>> queue;
    ska::bytell_hash_map<Vertex, Weight> bestDistance;
    Parents parent;
    Vertex bestVertex;
    Weight pS;
    // |adj| is a parameter which is filled by FillAdjacencyList(). |adj| and |stateV| are necessary
    // because IndexGraphStarterJoints (derived from AStarGraph) is not ready for
    // a repeated call of IndexGraphStarterJoints::GetEdgeList() with the same |vertexData| and
    // |isOutgoing| parameters. On the other hand after a call of AStarGraph::GetEdgeList()
    // it may be seen that it's time to stop two-thread route building in FindPathBidirectional(),
    // but the result of the call AStarGraph::GetEdgeList() (|adj|) and
    // the vertex which the method AStarGraph::GetEdgeList() is called for (|stateV|),
    // are still necessary for each context to finish route building in one thread step.
    // So they should be kept to continue route building correctly after two thread step.
    // If |stateV| has value it should be used instead of |queue.top()|. In that case
    // |adj| is filled with outgoing or ingoing edges. If not, |queue.top()| should be used.
    std::vector<Edge> adj;
    std::optional<State> stateV;
  };

  static void ReconstructPath(Vertex const & v,
                              typename BidirectionalStepContext::Parents const & parent,
                              std::vector<Vertex> & path);
  static void ReconstructPathBidirectional(
      Vertex const & v, Vertex const & w,
      typename BidirectionalStepContext::Parents const & parentV,
      typename BidirectionalStepContext::Parents const & parentW, std::vector<Vertex> & path);

  template <typename P>
  void FindPathBidirectionalTwoThreadStep(
      Weight const & epsilon, P & params,
      AStarAlgorithm<Vertex, Edge, Weight>::BidirectionalStepContext & forward,
      AStarAlgorithm<Vertex, Edge, Weight>::BidirectionalStepContext & backward) const;
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
    return graph.HeuristicCostEstimate(vertexFrom, finalVertex, true /* forward */) -
           graph.HeuristicCostEstimate(vertexTo, finalVertex, true /* forward */);
  };

  auto const fullToReducedLength = [&](Vertex const & vertexFrom, Vertex const & vertexTo,
                                       Weight const length) {
    return length - heuristicDiff(vertexFrom, vertexTo);
  };

  auto const reducedToFullLength = [&](Vertex const & vertexFrom, Vertex const & vertexTo,
                                       Weight const reducedLength) {
    return reducedLength + heuristicDiff(vertexFrom, vertexTo);
  };

  std::optional<std::mutex> dummy;
  auto visitVertex = [&](Vertex const & vertex) {
    if (periodicCancellable.IsCancelled())
    {
      resultCode = Result::Cancelled;
      return false;
    }

    params.m_onVisitedVertexCallback(vertex, finalVertex, true /* forward */, dummy);

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
    return params.m_checkLengthCallback(reducedToRealLength(state), true /* forward */);
  };

  PropagateWave(graph, startVertex, visitVertex, adjustEdgeWeight, filterStates,
                reducedToRealLength, context);

  if (resultCode == Result::OK)
  {
    context.ReconstructPath(finalVertex, result.m_path);
    result.m_distance =
        reducedToFullLength(startVertex, finalVertex, context.GetDistance(finalVertex));

    if (!params.m_checkLengthCallback(result.m_distance, true /* forward */))
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

  auto const useTwoThreads = graph.IsTwoThreadsReady();
  LOG(LINFO,
      (useTwoThreads ? "Bidirectional A* in two threads." : "Bidirectional A* in one thread."));
  std::optional<std::mutex> mtx;

  BidirectionalStepContext forward(mtx, true /* forward */, startVertex, finalVertex, graph);
  BidirectionalStepContext backward(mtx, false /* forward */, startVertex, finalVertex, graph);

  auto & forwardParents = forward.GetParents();
  auto & backwardParents = backward.GetParents();

  bool foundAnyPath = false;
  auto bestPathReducedLength = kZeroDistance;
  auto bestPathRealLength = kZeroDistance;

  forward.Init(startVertex);
  backward.Init(finalVertex);

  if (useTwoThreads)
    FindPathBidirectionalTwoThreadStep(epsilon, params, forward, backward);

  CHECK(!mtx.has_value(), ("Mutex should be destroyed."));

  // To use the search code both for backward and forward directions
  // we keep the pointers to everything related to the search in the
  // 'current' and 'next' directions. Swapping these pointers indicates
  // changing the end we are searching from.
  BidirectionalStepContext * cur = &forward;
  BidirectionalStepContext * nxt = &backward;

  auto const getResult = [&]() {
    if (!params.m_checkLengthCallback(bestPathRealLength, true /* forward */))
      return Result::NoPath;

    ReconstructPathBidirectional(cur->bestVertex, nxt->bestVertex, cur->parent, nxt->parent,
                                 result.m_path);
    result.m_distance = bestPathRealLength;
    CHECK(!result.m_path.empty(), ());
    if (!cur->forward)
      reverse(result.m_path.begin(), result.m_path.end());

    return Result::OK;
  };

  // It is not necessary to check emptiness for both queues here
  // because if we have not found a path by the time one of the
  // queues is exhausted, we never will.
  uint32_t steps = 0;
  PeriodicPollCancellable periodicCancellable(params.m_cancellable);

  // One thread step.
  while (cur->IsNextVertex() && nxt->IsNextVertex())
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

    // In case of two thread version it's necessary to process last edges got on two thread step.
    // The information is kept in |cur->stateV| and |cur->adj|.
    // |cur->stateV| should be checked before call |cur->TopAndPopState()|.
    auto const firstStepAfterTwoThreads = cur->stateV != std::nullopt;

    State const stateV = cur->TopAndPopState();

    if (cur->ExistsStateWithBetterDistance(stateV))
      continue;
    if (!firstStepAfterTwoThreads)
    {
      params.m_onVisitedVertexCallback(stateV.vertex, cur->GetEndVertex(), true /* forward */, mtx);
      cur->FillAdjacencyList(stateV);
    }

    auto const & pV = stateV.heuristic;
    for (auto const & edge : cur->adj)
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
      if (!params.m_checkLengthCallback(fullLength, true /* forward */))
        continue;

      if (cur->ExistsStateWithBetterDistance(stateW, epsilon))
        continue;

      stateW.heuristic = pW;
      cur->Update(stateW, stateV);

      if (auto op = nxt->GetDistance(stateW.vertex); op)
      {
        auto const & distW = *op;
        // Reduced length that the path we've just found has in the original graph:
        // find the reduced length of the path's parts in the reduced forward and backward graphs.
        auto const curPathReducedLength = stateW.distance + distW;
        // No epsilon here: it is ok to overshoot slightly.
        auto const connectible = graph.AreWavesConnectible(forwardParents, stateW.vertex,
                                                           backwardParents);
        if ((!foundAnyPath || bestPathReducedLength > curPathReducedLength) && connectible)
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

      if (stateW.vertex != cur->GetEndVertex())
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

  std::optional<std::mutex> dummy;
  auto visitVertex = [&](Vertex const & vertex) {

    if (periodicCancellable.IsCancelled())
    {
      wasCancelled = true;
      return false;
    }

    params.m_onVisitedVertexCallback(startVertex, vertex, true /* forward */, dummy);

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
    return params.m_checkLengthCallback(state.distance, true /* forward */);
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

template <typename Vertex, typename Edge, typename Weight>
template <typename P>
void AStarAlgorithm<Vertex, Edge, Weight>::FindPathBidirectionalTwoThreadStep(
    Weight const & epsilon, P & params,
    AStarAlgorithm<Vertex, Edge, Weight>::BidirectionalStepContext & forward,
    AStarAlgorithm<Vertex, Edge, Weight>::BidirectionalStepContext & backward) const
{
  CHECK(&*forward.mtx == &*backward.mtx,
        ("forward and backward waves have to contain the same mutex."));
  auto & mtx = forward.mtx;
  mtx.emplace();
  auto wave = [&epsilon, &params](BidirectionalStepContext & context,
                                  BidirectionalStepContext & oppositeContext,
                                  std::atomic<bool> & exit) {
    LOG(LINFO, ("---FindPath-------wave---------------------------",
        context.forward ? "forward" : "backward", "queue size:", context.queue.size(), "exit:", exit.load()));
    size_t i = 0;
    while (!context.queue.empty()  && !exit.load())
    {
      // Note. In case of exception in copy c-tor |context.queue| structure is not damaged
      // because the routing is stopped.
      State const stateV = context.queue.top();
      context.queue.pop();

      if (context.ExistsStateWithBetterDistance(stateV))
        continue;

      params.m_onVisitedVertexCallback(stateV.vertex, context.GetEndVertex(), context.forward, context.mtx);

      context.FillAdjacencyList(stateV);
      auto const & pV = stateV.heuristic;

      // To get outgoing or ingoing edges |context.FillAdjacencyList(stateV)| is called above.
      // Then some of got edges is places to the queue, but one edge should be places only once.
      // |toQueue| is needed to guaranty this in the monument of switching to one thread step.
      // It means that if forward and backward waves intersects on some edge, two thread
      // step should be stopped and this edge and edges after it according to the queue
      // should be processed with one thread step below.
      std::vector<State> toQueue;
      toQueue.reserve(context.adj.size());
      bool intersectsWithOtherWave = false;

      for (auto const & edge : context.adj)
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

        auto const fullLength = weight + stateV.distance + context.pS - pV;
        if (!params.m_checkLengthCallback(fullLength, context.forward))
          continue;

        if (context.ExistsStateWithBetterDistance(stateW, epsilon))
          continue;

        stateW.heuristic = pW;
        ++i;
        if (intersectsWithOtherWave || oppositeContext.GetDistance(stateW.vertex))
          intersectsWithOtherWave = true;

        toQueue.push_back(stateW);
      }
      if (intersectsWithOtherWave)
      {
        // |context.stateV| is set here and |context.adj| is already set.
        context.stateV = stateV;
        exit.store(true);
        LOG(LINFO, ("---FindPath-------wave end---------------------------",
            context.forward ? "forward" : "backward", i));
        return;
      }
      else
      {
        for (auto const & stateW : toQueue)
          context.UpdateAndPushIfNotEnd(stateW, stateV);
      }
    }

    LOG(LINFO, ("----FindPath------wave end---------------------------",
        context.forward ? "forward" : "backward",
        context.queue.empty() ? "queue is empty" : (exit.load() ? "Another wave is finished" : "STRANGE BEHAVIOR"), i));
    exit.store(true);
  };

  std::atomic<bool> shouldExit = false;
  PeriodicPollCancellable periodicCancellable(params.m_cancellable);

  // Starting two wave in two threads.
  threads::SimpleThread backwardWave(wave, std::ref(backward), std::ref(forward), std::ref(shouldExit));
  wave(forward, backward, shouldExit);
  backwardWave.join();

  LOG(LINFO, ("-------Two thread part of FindPath-------Finished-----------------"));
  mtx.reset();
}
}  // namespace routing
