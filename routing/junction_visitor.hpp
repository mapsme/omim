#pragma once

#include "routing/base/astar_progress.hpp"

#include "routing/router_delegate.hpp"

#include "geometry/point2d.hpp"

#include "base/optional_lock_guard.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

namespace routing
{
double constexpr kProgressInterval = 0.5;

template <typename Graph>
class JunctionVisitor
{
public:
  using Vertex = typename Graph::Vertex;

  JunctionVisitor(Graph & graph, RouterDelegate const & delegate, uint32_t visitPeriod,
                  std::shared_ptr<AStarProgress> const & progress = nullptr)
    : m_graph(graph), m_delegate(delegate), m_visitPeriod(visitPeriod), m_progress(progress)
  {
    if (progress)
      m_lastProgressPercent = progress->GetLastPercent();
  }

  void operator()(Vertex const & from, Vertex const & to, bool isOutgoing,
                  std::optional<std::mutex> & m)
  {
    if (!IncrementCounterAndCheckPeriod(isOutgoing ? m_visitCounter : m_visitCounterBwd))
      return;

    if (!m_graph.IsTwoThreadsReady())
      CHECK(!m.has_value(), ("Graph is not ready for two threads but there's a mutex."));

    auto const & pointFrom = m_graph.GetPoint(from, true /* front */, isOutgoing);
    auto const & pointTo = m_graph.GetPoint(to, true /* front */, isOutgoing);

    base::OptionalLockGuard guard(m);
    m_delegate.OnPointCheck(pointFrom);

    auto progress = m_progress.lock();
    if (!progress)
      return;

    auto const currentPercent = progress->UpdateProgress(pointFrom, pointTo);
    if (currentPercent - m_lastProgressPercent > kProgressInterval)
    {
      m_lastProgressPercent = currentPercent;
      m_delegate.OnProgress(currentPercent);
    }
  }

private:
  bool IncrementCounterAndCheckPeriod(uint32_t & counter)
  {
    ++counter;
    if (counter % m_visitPeriod == 0)
      return true;
    return false;
  }

  Graph & m_graph;
  RouterDelegate const & m_delegate;
  // Only one counter is used depending on one or two thread version.
  uint32_t m_visitCounter = 0;
  uint32_t m_visitCounterBwd = 0;
  uint32_t m_visitPeriod;
  std::weak_ptr<AStarProgress> m_progress;
  double m_lastProgressPercent = 0.0;
};
}  // namespace routing
