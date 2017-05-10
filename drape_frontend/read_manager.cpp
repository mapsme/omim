#include "drape_frontend/read_manager.hpp"
#include "drape_frontend/message_subclasses.hpp"
#include "drape_frontend/visual_params.hpp"

#include "platform/platform.hpp"

#include "base/buffer_vector.hpp"
#include "base/stl_add.hpp"

#include <algorithm>
#include <functional>
#include <iterator>

namespace df
{

namespace
{

struct LessCoverageCell
{
  bool operator()(std::shared_ptr<TileInfo> const & l, TileKey const & r) const
  {
    return l->GetTileKey() < r;
  }

  bool operator()(TileKey const & l, std::shared_ptr<TileInfo> const & r) const
  {
    return l < r->GetTileKey();
  }

  bool operator()(std::shared_ptr<TileInfo> const & l, std::shared_ptr<TileInfo> const & r) const
  {
    return l->GetTileKey() < r->GetTileKey();
  }
};

} // namespace

ReadManager::ReadManager(ref_ptr<ThreadsCommutator> commutator, MapDataProvider & model,
                         bool allow3dBuildings, bool trafficEnabled)
  : m_commutator(commutator)
  , m_model(model)
  , m_pool(make_unique_dp<threads::ThreadPool>(ReadCount(), std::bind(&ReadManager::OnTaskFinished, this, std::placeholders::_1)))
  , m_have3dBuildings(false)
  , m_allow3dBuildings(allow3dBuildings)
  , m_trafficEnabled(trafficEnabled)
  , m_modeChanged(false)
  , myPool(64, ReadMWMTaskFactory(m_model))
  , m_counter(0)
  , m_generationCounter(0)
{
}

void ReadManager::OnTaskFinished(threads::IRoutine * task)
{
  ASSERT(dynamic_cast<ReadMWMTask *>(task) != NULL, ());
  ReadMWMTask * t = static_cast<ReadMWMTask *>(task);

  // finish tiles
  {
    std::lock_guard<std::mutex> lock(m_finishedTilesMutex);

    m_activeTiles.erase(t->GetTileKey());

    // decrement counter
    ASSERT(m_counter > 0, ());
    --m_counter;
    if (m_counter == 0)
    {
      m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                make_unique_dp<FinishReadingMessage>(),
                                MessagePriority::Normal);
    }

    if (!task->IsCancelled())
    {
      TTilesCollection tiles;
      tiles.emplace(t->GetTileKey());
      m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                make_unique_dp<FinishTileReadMessage>(move(tiles)),
                                MessagePriority::Normal);
    }
  }

  t->Reset();
  myPool.Return(t);
}

void ReadManager::UpdateCoverage(ScreenBase const & screen, bool have3dBuildings, bool forceUpdate,
                                 TTilesCollection const & tiles, ref_ptr<dp::TextureManager> texMng)
{
  m_modeChanged |= (m_have3dBuildings != have3dBuildings);
  m_have3dBuildings = have3dBuildings;

  if (m_modeChanged || forceUpdate || MustDropAllTiles(screen))
  {
    m_modeChanged = false;

    std::for_each(m_tileInfos.begin(), m_tileInfos.end(), std::bind(&ReadManager::CancelTileInfo, this, std::placeholders::_1));
    m_tileInfos.clear();

    IncreaseCounter(static_cast<int>(tiles.size()));
    m_generationCounter++;

    std::for_each(tiles.begin(), tiles.end(), std::bind(&ReadManager::PushTaskBackForTileKey, this, std::placeholders::_1, texMng));
  }
  else
  {
    // Find rects that go out from viewport
    TTileInfoCollection outdatedTiles;
#ifdef _MSC_VER
    vs_bug::
#endif
    std::set_difference(m_tileInfos.begin(), m_tileInfos.end(),
                   tiles.begin(), tiles.end(),
                   std::back_inserter(outdatedTiles), LessCoverageCell());

    std::for_each(outdatedTiles.begin(), outdatedTiles.end(), std::bind(&ReadManager::ClearTileInfo, this, std::placeholders::_1));

    // Find rects that go in into viewport.
    buffer_vector<TileKey, 8> newTiles;
#ifdef _MSC_VER
    vs_bug::
#endif
    std::set_difference(tiles.begin(), tiles.end(),
                   m_tileInfos.begin(), m_tileInfos.end(),
                   std::back_inserter(newTiles), LessCoverageCell());

    // Find ready tiles.
    TTileInfoCollection readyTiles;
#ifdef _MSC_VER
    vs_bug::
#endif
    std::set_difference(m_tileInfos.begin(), m_tileInfos.end(),
                   outdatedTiles.begin(), outdatedTiles.end(),
                   std::back_inserter(readyTiles), LessCoverageCell());

    IncreaseCounter(static_cast<int>(newTiles.size()));
    CheckFinishedTiles(readyTiles);
    std::for_each(newTiles.begin(), newTiles.end(), std::bind(&ReadManager::PushTaskBackForTileKey, this, std::placeholders::_1, texMng));
  }

  m_currentViewport = screen;
}

void ReadManager::Invalidate(TTilesCollection const & keyStorage)
{
  TTileSet tilesToErase;
  for (auto const & info : m_tileInfos)
  {
    if (keyStorage.find(info->GetTileKey()) != keyStorage.end())
      tilesToErase.insert(info);
  }

  for (auto const & info : tilesToErase)
  {
    CancelTileInfo(info);
    m_tileInfos.erase(info);
  }
}

void ReadManager::InvalidateAll()
{
  for (auto const & info : m_tileInfos)
    CancelTileInfo(info);

  m_tileInfos.clear();

  m_modeChanged = true;
}

void ReadManager::Stop()
{
  std::for_each(m_tileInfos.begin(), m_tileInfos.end(), std::bind(&ReadManager::CancelTileInfo, this, std::placeholders::_1));
  m_tileInfos.clear();

  m_pool->Stop();
  m_pool.reset();
}

bool ReadManager::CheckTileKey(TileKey const & tileKey) const
{
  for (auto const & tileInfo : m_tileInfos)
    if (tileInfo->GetTileKey() == tileKey)
      return !tileInfo->IsCancelled();

  return false;
}

uint32_t ReadManager::ReadCount()
{
  return std::max(static_cast<int>(GetPlatform().CpuCores()) - 2, 2);
}

bool ReadManager::MustDropAllTiles(ScreenBase const & screen) const
{
  int const oldScale = df::GetDrawTileScale(m_currentViewport);
  int const newScale = df::GetDrawTileScale(screen);
  return (oldScale != newScale) || !m_currentViewport.GlobalRect().IsIntersect(screen.GlobalRect());
}

void ReadManager::PushTaskBackForTileKey(TileKey const & tileKey, ref_ptr<dp::TextureManager> texMng)
{
  std::shared_ptr<TileInfo> tileInfo(new TileInfo(make_unique_dp<EngineContext>(TileKey(tileKey, m_generationCounter),
                                             m_commutator, texMng), m_customSymbolsContext));
  tileInfo->Set3dBuildings(m_have3dBuildings && m_allow3dBuildings);
  tileInfo->SetTrafficEnabled(m_trafficEnabled);
  m_tileInfos.insert(tileInfo);
  ReadMWMTask * task = myPool.Get();

  task->Init(tileInfo);
  {
    std::lock_guard<std::mutex> lock(m_finishedTilesMutex);
    m_activeTiles.insert(tileKey);
  }
  m_pool->PushBack(task);
}

void ReadManager::CheckFinishedTiles(TTileInfoCollection const & requestedTiles)
{
  if (requestedTiles.empty())
    return;

  TTilesCollection finishedTiles;

  std::lock_guard<std::mutex> lock(m_finishedTilesMutex);

  for (auto const & tile : requestedTiles)
    if (m_activeTiles.find(tile->GetTileKey()) == m_activeTiles.end())
      finishedTiles.emplace(tile->GetTileKey());

  if (!finishedTiles.empty())
  {
    m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                              make_unique_dp<FinishTileReadMessage>(move(finishedTiles)),
                              MessagePriority::Normal);
  }
}

void ReadManager::CancelTileInfo(std::shared_ptr<TileInfo> const & tileToCancel)
{
  tileToCancel->Cancel();
}

void ReadManager::ClearTileInfo(std::shared_ptr<TileInfo> const & tileToClear)
{
  CancelTileInfo(tileToClear);
  m_tileInfos.erase(tileToClear);
}

void ReadManager::IncreaseCounter(int value)
{
  std::lock_guard<std::mutex> lock(m_finishedTilesMutex);
  m_counter += value;

  if (m_counter == 0)
  {
    m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                              make_unique_dp<FinishReadingMessage>(),
                              MessagePriority::Normal);
  }
}

void ReadManager::Allow3dBuildings(bool allow3dBuildings)
{
  if (m_allow3dBuildings != allow3dBuildings)
  {
    m_modeChanged = true;
    m_allow3dBuildings = allow3dBuildings;
  }
}

void ReadManager::SetTrafficEnabled(bool trafficEnabled)
{
  if (m_trafficEnabled != trafficEnabled)
  {
    m_modeChanged = true;
    m_trafficEnabled = trafficEnabled;
  }
}

void ReadManager::UpdateCustomSymbols(CustomSymbols const & symbols)
{
  CustomSymbols currentSymbols = m_customSymbolsContext ? m_customSymbolsContext->m_symbols :
                                 CustomSymbols();
  for (auto const & s : symbols)
    currentSymbols[s.first] = s.second;
  m_customSymbolsContext = std::make_shared<CustomSymbolsContext>(std::move(currentSymbols));
}

void ReadManager::RemoveCustomSymbols(MwmSet::MwmId const & mwmId, std::vector<FeatureID> & leftoverIds)
{
  if (!m_customSymbolsContext)
    return;

  CustomSymbols currentSymbols;
  leftoverIds.reserve(m_customSymbolsContext->m_symbols.size());
  for (auto const & s : m_customSymbolsContext->m_symbols)
  {
    if (s.first.m_mwmId != mwmId)
    {
      currentSymbols.insert(std::make_pair(s.first, s.second));
      leftoverIds.push_back(s.first);
    }
  }
  m_customSymbolsContext = std::make_shared<CustomSymbolsContext>(std::move(currentSymbols));
}

void ReadManager::RemoveAllCustomSymbols()
{
  m_customSymbolsContext = std::make_shared<CustomSymbolsContext>(CustomSymbols());
}

} // namespace df
