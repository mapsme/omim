#include "drape_frontend/read_manager.hpp"
#include "drape_frontend/message_subclasses.hpp"
#include "drape_frontend/visual_params.hpp"

#include "platform/platform.hpp"

#include "base/buffer_vector.hpp"
#include "base/stl_add.hpp"

#include "std/bind.hpp"
#include "std/algorithm.hpp"

namespace df
{

namespace
{

struct LessCoverageCell
{
  bool operator()(shared_ptr<TileInfo> const & l, TileKey const & r) const
  {
    return l->GetTileKey() < r;
  }

  bool operator()(TileKey const & l, shared_ptr<TileInfo> const & r) const
  {
    return l < r->GetTileKey();
  }
};

} // namespace

ReadManager::ReadManager(ref_ptr<ThreadsCommutator> commutator, MapDataProvider & model, bool allow3dBuildings)
  : m_commutator(commutator)
  , m_model(model)
  , m_pool(make_unique_dp<threads::ThreadPool>(ReadCount(), bind(&ReadManager::OnTaskFinished, this, _1)))
  , m_forceUpdate(true)
  , m_need3dBuildings(false)
  , m_allow3dBuildings(allow3dBuildings)
  , m_modeChanged(false)
  , myPool(64, ReadMWMTaskFactory(m_memIndex, m_model))
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
    lock_guard<mutex> lock(m_finishedTilesMutex);

    // add finished tile to collection
    m_finishedTiles.emplace(t->GetTileKey());

    // decrement counter
    ASSERT(m_counter > 0, ());
    --m_counter;
    if (m_counter == 0)
    {
      m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                make_unique_dp<FinishReadingMessage>(m_finishedTiles),
                                MessagePriority::Normal);
      m_finishedTiles.clear();
    }
  }

  t->Reset();
  myPool.Return(t);
}

void ReadManager::UpdateCoverage(ScreenBase const & screen, bool is3dBuildings,
                                 TTilesCollection const & tiles, ref_ptr<dp::TextureManager> texMng)
{
  if (screen == m_currentViewport && !m_forceUpdate)
    return;

  m_forceUpdate = false;
  m_modeChanged |= m_need3dBuildings != is3dBuildings;
  m_need3dBuildings = is3dBuildings;

  if (m_modeChanged || MustDropAllTiles(screen))
  {
    m_modeChanged = false;

    IncreaseCounter(static_cast<int>(tiles.size()));
    m_generationCounter++;

    for_each(m_tileInfos.begin(), m_tileInfos.end(), bind(&ReadManager::CancelTileInfo, this, _1));
    m_tileInfos.clear();
    for_each(tiles.begin(), tiles.end(), bind(&ReadManager::PushTaskBackForTileKey, this, _1, texMng));
  }
  else
  {
    // Find rects that go out from viewport
    buffer_vector<shared_ptr<TileInfo>, 8> outdatedTiles;
#ifdef _MSC_VER
    vs_bug::
#endif
    set_difference(m_tileInfos.begin(), m_tileInfos.end(),
                   tiles.begin(), tiles.end(),
                   back_inserter(outdatedTiles), LessCoverageCell());

    // Find rects that go in into viewport.
    buffer_vector<TileKey, 8> inputRects;
#ifdef _MSC_VER
    vs_bug::
#endif
    set_difference(tiles.begin(), tiles.end(),
                   m_tileInfos.begin(), m_tileInfos.end(),
                   back_inserter(inputRects), LessCoverageCell());

    // Find tiles which must be re-read.
    buffer_vector<shared_ptr<TileInfo>, 8> rereadTiles;
    for (shared_ptr<TileInfo> const & tile : m_tileInfos)
    {
      for (shared_ptr<TileInfo> & outTile : outdatedTiles)
      {
        if (IsNeighbours(tile->GetTileKey(), outTile->GetTileKey()))
        {
          rereadTiles.push_back(tile);
          break;
        }
      }
    }

    IncreaseCounter(static_cast<int>(inputRects.size() + rereadTiles.size()));

    for_each(outdatedTiles.begin(), outdatedTiles.end(), bind(&ReadManager::ClearTileInfo, this, _1));
    for_each(rereadTiles.begin(), rereadTiles.end(), bind(&ReadManager::PushTaskFront, this, _1));
    for_each(inputRects.begin(), inputRects.end(), bind(&ReadManager::PushTaskBackForTileKey, this, _1, texMng));
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

  m_forceUpdate = true;
}

void ReadManager::InvalidateAll()
{
  for (auto const & info : m_tileInfos)
    CancelTileInfo(info);

  m_tileInfos.clear();

  m_forceUpdate = true;
  m_modeChanged = true;
}

void ReadManager::Stop()
{
  for_each(m_tileInfos.begin(), m_tileInfos.end(), bind(&ReadManager::CancelTileInfo, this, _1));
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

size_t ReadManager::ReadCount()
{
  return max(static_cast<int>(GetPlatform().CpuCores()) - 2, 1);
}

bool ReadManager::MustDropAllTiles(ScreenBase const & screen) const
{
  int const oldScale = df::GetDrawTileScale(m_currentViewport);
  int const newScale = df::GetDrawTileScale(screen);
  return (oldScale != newScale) || !m_currentViewport.GlobalRect().IsIntersect(screen.GlobalRect());
}

void ReadManager::PushTaskBackForTileKey(TileKey const & tileKey, ref_ptr<dp::TextureManager> texMng)
{
  shared_ptr<TileInfo> tileInfo(new TileInfo(make_unique_dp<EngineContext>(TileKey(tileKey, m_generationCounter),
                                                                           m_commutator, texMng)));
  tileInfo->Set3dBuildings(m_need3dBuildings && m_allow3dBuildings);
  m_tileInfos.insert(tileInfo);
  ReadMWMTask * task = myPool.Get();
  task->Init(tileInfo);
  m_pool->PushBack(task);
}

void ReadManager::PushTaskFront(shared_ptr<TileInfo> const & tileToReread)
{
  tileToReread->Set3dBuildings(m_need3dBuildings && m_allow3dBuildings);
  ReadMWMTask * task = myPool.Get();
  task->Init(tileToReread);
  m_pool->PushFront(task);
}

void ReadManager::CancelTileInfo(shared_ptr<TileInfo> const & tileToCancel)
{
  tileToCancel->Cancel(m_memIndex);
}

void ReadManager::ClearTileInfo(shared_ptr<TileInfo> const & tileToClear)
{
  CancelTileInfo(tileToClear);
  m_tileInfos.erase(tileToClear);
}

void ReadManager::IncreaseCounter(int value)
{
  lock_guard<mutex> lock(m_finishedTilesMutex);
  m_counter += value;
}

void ReadManager::Allow3dBuildings(bool allow3dBuildings)
{
  if (m_allow3dBuildings != allow3dBuildings)
  {
    m_modeChanged = true;
    m_forceUpdate = true;
    m_allow3dBuildings = allow3dBuildings;
  }
}

} // namespace df
