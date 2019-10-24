#pragma once

#include "search/result.hpp"
#include "search/search_params.hpp"

#include "base/logging.hpp"

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
  }

  bool AddResult(Result && res) { return m_results.AddResult(std::move(res)); }
  void AddResultNoChecks(Result && res) { m_results.AddResultNoChecks(std::move(res)); }

  void AddBookmarkResult(bookmarks::Result const & result) { m_results.AddBookmarkResult(result); }

  void Emit()
  {
    if (m_prevEmitSize == m_results.GetCount())
      return;
    m_prevEmitSize = m_results.GetCount();

    if (m_onResults)
      m_onResults(m_results);
    else
      LOG(LERROR, ("OnResults is not set."));
  }

  Results const & GetResults() const { return m_results; }

  void Finish(bool cancelled)
  {
    m_results.SetEndMarker(cancelled);
    if (m_onResults)
      m_onResults(m_results);
    else
      LOG(LERROR, ("OnResults is not set."));
  }

private:
  SearchParams::OnResults m_onResults;
  Results m_results;
  size_t m_prevEmitSize = 0;
};
}  // namespace search
