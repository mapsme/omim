#pragma once

#include "indexer/index.hpp"

#include "base/macros.hpp"

#include <map>
#include <memory>

namespace search
{
class RankTable;

class RankTableCache
{
  using TId = MwmSet::MwmId;

  struct TKey : public MwmSet::MwmHandle
  {
    TKey() = default;
    TKey(TKey &&) = default;

    explicit TKey(TId const & id) { this->m_mwmId = id; }
    explicit TKey(MwmSet::MwmHandle && handle) : MwmSet::MwmHandle(move(handle)) {}
  };

public:
  RankTableCache() = default;

  RankTable const & Get(Index & index, TId const & mwmId);

  void Remove(TId const & id);
  void Clear();

private:
  struct Compare
  {
    bool operator()(TKey const & r1, TKey const & r2) const { return (r1.GetId() < r2.GetId()); }
  };

  std::map<TKey, std::unique_ptr<RankTable>, Compare> m_ranks;

  DISALLOW_COPY_AND_MOVE(RankTableCache);
};

}  // namespace search
