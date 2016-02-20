#include "drape_frontend/overlay_batcher.hpp"

#include "drape_frontend/map_shape.hpp"

#include "drape/batcher.hpp"
#include "drape/render_bucket.hpp"
#include "drape/texture_manager.hpp"

namespace df
{

uint32_t const kOverlayIndexBufferSize = 30000;
uint32_t const kOverlayVertexBufferSize = 20000;

OverlayBatcher::OverlayBatcher(TileKey const & key)
  : m_batcher(kOverlayIndexBufferSize, kOverlayVertexBufferSize)
{
  int const kAverageRenderDataCount = 5;
  m_data.reserve(kAverageRenderDataCount);

  m_batcher.StartSession([this, key](dp::GLState const & state, drape_ptr<dp::RenderBucket> && bucket)
  {
    FlushGeometry(key, state, move(bucket));
  });
}

void OverlayBatcher::Batch(drape_ptr<MapShape> const & shape, ref_ptr<dp::TextureManager> texMng)
{
  m_batcher.SetFeatureMinZoom(shape->GetFeatureMinZoom());
  bool const sharedFeature = shape->GetFeatureInfo().IsValid();
  if (sharedFeature)
    m_batcher.StartFeatureRecord(shape->GetFeatureInfo(), shape->GetFeatureLimitRect());
  shape->Draw(make_ref(&m_batcher), texMng);
  if (sharedFeature)
    m_batcher.EndFeatureRecord();
}

void OverlayBatcher::Finish(TOverlaysRenderData & data)
{
  m_batcher.EndSession();
  data.swap(m_data);
}

void OverlayBatcher::FlushGeometry(TileKey const & key, dp::GLState const & state, drape_ptr<dp::RenderBucket> && bucket)
{
  m_data.emplace_back(key, state, move(bucket));
}

} // namespace df
