#pragma once

#include "search/result.hpp"
#include "search/search_params.hpp"

#include "base/logging.hpp"
#include "base/timer.hpp"

#include <utility>

namespace search
{
namespace bookmarks
{
struct Result;
}

class Emitter
{
public:
  void Init(SearchParams::OnResults onResults)
  {
    m_onResults = onResults;
    m_results.Clear();
    m_prevEmitSize = 0;
    m_timer.Reset();
  }

  bool AddResult(Result && res) { return m_results.AddResult(std::move(res)); }
  void AddResultNoChecks(Result && res) { m_results.AddResultNoChecks(std::move(res)); }

  void AddBookmarkResult(bookmarks::Result const & result) { m_results.AddBookmarkResult(result); }

  void Emit(bool force = false)
  {
    if (m_prevEmitSize == m_results.GetCount() && !force)
      return;
    m_prevEmitSize = m_results.GetCount();

    LOG(LINFO, ("Emitting a new batch of results. Time since search start:",
                m_timer.ElapsedSeconds(), "seconds."));

    if (m_onResults)
      m_onResults(m_results);
    else
      LOG(LERROR, ("OnResults is not set."));
  }

  Results const & GetResults() const { return m_results; }

  void Finish(bool cancelled)
  {
    m_results.SetEndMarker(cancelled);
    Emit(true /* force */);
  }

private:
  SearchParams::OnResults m_onResults;
  Results m_results;
  size_t m_prevEmitSize = 0;
  base::Timer m_timer;
};
}  // namespace search
