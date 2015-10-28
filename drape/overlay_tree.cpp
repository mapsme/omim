#include "drape/overlay_tree.hpp"

#include "std/bind.hpp"

namespace dp
{

int const FRAME_UPDATE_PERIOD = 10;

void OverlayTree::Frame()
{
  m_frameCounter++;
  if (m_frameCounter >= FRAME_UPDATE_PERIOD)
    m_frameCounter = -1;
}

bool OverlayTree::IsNeedUpdate() const
{
  return m_frameCounter == -1;
}

void OverlayTree::ForceUpdate()
{
  Clear();
  m_frameCounter = -1;
}

void OverlayTree::StartOverlayPlacing(ScreenBase const & screen)
{
  ASSERT(IsNeedUpdate(), ());
  Clear();
  m_traits.m_modelView = screen;
}

void OverlayTree::Add(ref_ptr<OverlayHandle> handle, bool isTransparent)
{
  ScreenBase const & modelView = GetModelView();
  bool const is3dMode = modelView.isPerspective();

  handle->SetIsVisible(false);

  if (!handle->Update(modelView))
    return;

  m2::RectD const pixelRect = handle->GetPixelRect(modelView, is3dMode);

  if (!modelView.PixelRect().IsIntersect(handle->GetPixelRect(modelView, false)) ||
      (is3dMode && !modelView.PixelRectIn3d().IsIntersect(pixelRect)))
  {
    handle->SetIsVisible(false);
    return;
  }

  typedef buffer_vector<detail::OverlayInfo, 8> OverlayContainerT;
  OverlayContainerT elements;
  /*
   * Find elements that already on OverlayTree and it's pixel rect
   * intersect with handle pixel rect ("Intersected elements")
   */
  ForEachInRect(pixelRect, [&] (detail::OverlayInfo const & info)
  {
    if (isTransparent == info.m_isTransparent && handle->IsIntersect(modelView, info.m_handle))
      elements.push_back(info);
  });

  double const inputPriority = handle->GetPriority();
  double const posY = is3dMode ? handle->GetPivot(modelView, true).y : 0.0;
  /*
   * In this loop we decide which element must be visible
   * If input element "handle" more priority than all "Intersected elements"
   * than we remove all "Intersected elements" and insert input element "handle"
   * But if some of already inserted elements more priority than we don't insert "handle"
   */
  for (OverlayContainerT::const_iterator it = elements.begin(); it != elements.end(); ++it)
  {
    bool const rejectByDepth = is3dMode ? posY > it->m_handle->GetPivot(modelView, true).y : false;
    if (inputPriority < it->m_handle->GetPriority() || rejectByDepth)
      return;
  }

  for (OverlayContainerT::const_iterator it = elements.begin(); it != elements.end(); ++it)
    Erase(*it);

  BaseT::Add(detail::OverlayInfo(handle, isTransparent), pixelRect);
}

void OverlayTree::EndOverlayPlacing()
{
  ForEach([] (detail::OverlayInfo const & info)
  {
    info.m_handle->SetIsVisible(true);
  });
}

void OverlayTree::Select(m2::RectD const & rect, TSelectResult & result) const
{
  ScreenBase screen = m_traits.m_modelView;
  ForEachInRect(rect, [&](detail::OverlayInfo const & info)
  {
    if (info.m_handle->IsVisible() && info.m_handle->GetFeatureID().IsValid())
    {
      OverlayHandle::Rects shape;
      info.m_handle->GetPixelShape(screen, shape, screen.isPerspective());
      for (m2::RectF const & rShape : shape)
      {
        if (rShape.IsIntersect(m2::RectF(rect)))
        {
          result.push_back(info.m_handle);
          break;
        }
      }
    }
  });
}

} // namespace dp
