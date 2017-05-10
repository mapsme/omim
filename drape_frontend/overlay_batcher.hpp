#pragma once

#include "drape/batcher.hpp"
#include "drape/glstate.hpp"
#include "drape/pointers.hpp"

#include "drape_frontend/tile_key.hpp"

#include <vector>

namespace dp
{
class TextureManager;
class RenderBucket;
} // namespace dp

namespace df
{

class MapShape;

struct OverlayRenderData
{
  TileKey m_tileKey;
  dp::GLState m_state;
  drape_ptr<dp::RenderBucket> m_bucket;

  OverlayRenderData(TileKey const & key, dp::GLState const & state, drape_ptr<dp::RenderBucket> && bucket)
    : m_tileKey(key), m_state(state), m_bucket(move(bucket))
  {}
};

using TOverlaysRenderData = std::vector<OverlayRenderData>;

class OverlayBatcher
{
public:
  OverlayBatcher(TileKey const & key);
  void Batch(drape_ptr<MapShape> const & shape, ref_ptr<dp::TextureManager> texMng);
  void Finish(TOverlaysRenderData & data);

private:
  void FlushGeometry(TileKey const & key, dp::GLState const & state, drape_ptr<dp::RenderBucket> && bucket);

  dp::Batcher m_batcher;
  TOverlaysRenderData m_data;
};

} // namespace df
