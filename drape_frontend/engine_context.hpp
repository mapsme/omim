#pragma once

#include "drape_frontend/map_shape.hpp"
#include "drape_frontend/tile_utils.hpp"
#include "drape_frontend/threads_commutator.hpp"

#include "drape/pointers.hpp"

namespace dp
{
class TextureManager;
} // namespace dp

namespace df
{

class Message;

class EngineContext
{
public:
  EngineContext(TileKey tileKey, ref_ptr<ThreadsCommutator> commutator,
                ref_ptr<dp::TextureManager> texMng);

  TileKey const & GetTileKey() const { return m_tileKey; }

  ref_ptr<dp::TextureManager> GetTextureManager() const;

  void BeginReadTile();
  void Flush(TMapShapes && shapes);
  void EndReadTile();

private:
  void PostMessage(drape_ptr<Message> && message);

  TileKey m_tileKey;
  ref_ptr<ThreadsCommutator> m_commutator;
  ref_ptr<dp::TextureManager> m_texMng;
};

} // namespace df
