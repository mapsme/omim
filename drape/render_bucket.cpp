#include "drape/render_bucket.hpp"

#include "drape/attribute_buffer_mutator.hpp"
#include "drape/debug_rect_renderer.hpp"
#include "drape/overlay_handle.hpp"
#include "drape/overlay_tree.hpp"
#include "drape/vertex_array_buffer.hpp"

#include "base/stl_add.hpp"
#include "std/bind.hpp"

namespace dp
{

RenderBucket::RenderBucket(drape_ptr<VertexArrayBuffer> && buffer)
  : m_buffer(move(buffer))
{
}

RenderBucket::~RenderBucket()
{
}

ref_ptr<VertexArrayBuffer> RenderBucket::GetBuffer()
{
  return make_ref(m_buffer);
}

drape_ptr<VertexArrayBuffer> && RenderBucket::MoveBuffer()
{
  return move(m_buffer);
}

size_t RenderBucket::GetOverlayHandlesCount() const
{
  return m_overlay.size();
}

drape_ptr<OverlayHandle> RenderBucket::PopOverlayHandle()
{
  ASSERT(!m_overlay.empty(), ());
  size_t lastElement = m_overlay.size() - 1;
  swap(m_overlay[0], m_overlay[lastElement]);
  drape_ptr<OverlayHandle> h = move(m_overlay[lastElement]);
  m_overlay.pop_back();
  return h;
}

ref_ptr<OverlayHandle> RenderBucket::GetOverlayHandle(size_t index)
{
  return make_ref(m_overlay[index]);
}

void RenderBucket::AddOverlayHandle(drape_ptr<OverlayHandle> && handle)
{
  m_overlay.push_back(move(handle));
}

void RenderBucket::Update(ScreenBase const & modelView)
{
  for (drape_ptr<OverlayHandle> & overlayHandle : m_overlay)
  {
    if (overlayHandle->IsVisible())
      overlayHandle->Update(modelView);
  }
}

void RenderBucket::CollectOverlayHandles(ref_ptr<OverlayTree> tree)
{
  for (drape_ptr<OverlayHandle> const & overlayHandle : m_overlay)
    tree->Add(make_ref(overlayHandle));
}

void RenderBucket::RemoveOverlayHandles(ref_ptr<OverlayTree> tree)
{
  for (drape_ptr<OverlayHandle> const & overlayHandle : m_overlay)
    tree->Remove(make_ref(overlayHandle));
}

void RenderBucket::Render()
{
  ASSERT(m_buffer != nullptr, ());

  if (!m_overlay.empty())
  {
    // in simple case when overlay is symbol each element will be contains 6 indexes
    AttributeBufferMutator attributeMutator;
    IndexBufferMutator indexMutator(6 * m_overlay.size());
    ref_ptr<IndexBufferMutator> rfpIndex = make_ref(&indexMutator);
    ref_ptr<AttributeBufferMutator> rfpAttrib = make_ref(&attributeMutator);

    bool hasIndexMutation = false;
    for (drape_ptr<OverlayHandle> const & handle : m_overlay)
    {
      if (handle->IndexesRequired())
      {
        if (handle->IsVisible())
          handle->GetElementIndexes(rfpIndex);
        hasIndexMutation = true;
      }

      if (handle->HasDynamicAttributes())
        handle->GetAttributeMutation(rfpAttrib);
    }

    m_buffer->ApplyMutation(hasIndexMutation ? rfpIndex : nullptr, rfpAttrib);
  }
  m_buffer->Render();
}

void RenderBucket::SetFeatureMinZoom(int minZoom)
{
  if (minZoom < m_featuresMinZoom)
    m_featuresMinZoom = minZoom;
}

void RenderBucket::StartFeatureRecord(FeatureGeometryId feature, const m2::RectD & limitRect)
{
  m_featureInfo = make_pair(feature, FeatureGeometryInfo(limitRect));
  m_buffer->ResetChangingTracking();
}

void RenderBucket::EndFeatureRecord(bool featureCompleted)
{
  ASSERT(m_featureInfo.first.IsValid(), ());
  m_featureInfo.second.m_featureCompleted = featureCompleted;
  if (m_buffer->IsChanged())
    m_featuresGeometryInfo.insert(m_featureInfo);
  m_featureInfo = TFeatureInfo();
}

void RenderBucket::AddFeaturesInfo(RenderBucket const & bucket)
{
  for (auto const & info : bucket.m_featuresGeometryInfo)
    m_featuresGeometryInfo.insert(info);
}

bool RenderBucket::IsFeaturesWaiting(TCheckFeaturesWaiting isFeaturesWaiting)
{
  ASSERT(IsShared(), ());
  for (auto const & featureRange : m_featuresGeometryInfo)
    if (isFeaturesWaiting(featureRange.second.m_limitRect))
      return true;
  return false;
}

void RenderBucket::RenderDebug(ScreenBase const & screen) const
{
#ifdef RENDER_DEBUG_RECTS
  if (!m_overlay.empty())
  {
    for (auto const & handle : m_overlay)
    {
      if (!screen.PixelRect().IsIntersect(handle->GetPixelRect(screen, false)))
        continue;

      OverlayHandle::Rects rects;
      handle->GetExtendedPixelShape(screen, rects);
      for (auto const & rect : rects)
      {
        if (screen.isPerspective() && !screen.PixelRectIn3d().IsIntersect(m2::RectD(rect)))
          continue;

        DebugRectRenderer::Instance().DrawRect(screen, rect, handle->IsVisible() ?
                                               dp::Color::Green() : dp::Color::Red());
      }
    }
  }
#endif
}

} // namespace dp
