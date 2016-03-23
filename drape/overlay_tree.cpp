#include "drape/overlay_tree.hpp"

#include "std/algorithm.hpp"
#include "std/bind.hpp"

namespace dp
{

int const kFrameUpdarePeriod = 10;
int const kFrameUpdarePeriodIn3d = 30;
int const kAverageHandlesCount[dp::OverlayRanksCount] = { 300, 200, 100 };

using TOverlayContainer = buffer_vector<detail::OverlayInfo, 8>;

namespace
{

class HandleComparator
{
public:
  HandleComparator(bool followingMode) : m_followingMode(followingMode) {}

  bool operator()(ref_ptr<OverlayHandle> const & l, ref_ptr<OverlayHandle> const & r) const
  {  
    return IsGreater(l, r);
  }

  bool IsGreater(ref_ptr<OverlayHandle> const & l, ref_ptr<OverlayHandle> const & r) const
  {
    uint64_t const mask = l->GetPriorityMask() & r->GetPriorityMask();
    uint64_t const priorityLeft = (m_followingMode ? l->GetPriorityInFollowingMode() :
                                                     l->GetPriority()) & mask;
    uint64_t const priorityRight = (m_followingMode ? r->GetPriorityInFollowingMode() :
                                                      r->GetPriority()) & mask;
    if (priorityLeft > priorityRight)
      return true;

    if (priorityLeft == priorityRight)
    {
      auto const & hashLeft = l->GetFeatureID();
      auto const & hashRight = r->GetFeatureID();
      if (hashLeft < hashRight)
        return true;

      if (hashLeft == hashRight)
        return l.get() < r.get();
    }

    return false;
  }

private:
  bool m_followingMode;
};

} // namespace

OverlayTree::OverlayTree()
  : m_frameCounter(-1)
  , m_followingMode(false)
{
  for (size_t i = 0; i < m_handles.size(); i++)
    m_handles[i].reserve(kAverageHandlesCount[i]);
}

bool OverlayTree::Frame(bool is3d)
{
  if (IsNeedUpdate())
    return true;

  m_frameCounter++;
  if (m_frameCounter >= (is3d ? kFrameUpdarePeriodIn3d : kFrameUpdarePeriod))
    m_frameCounter = -1;

  return IsNeedUpdate();
}

bool OverlayTree::IsNeedUpdate() const
{
  return m_frameCounter == -1;
}

void OverlayTree::ForceUpdate()
{
  m_frameCounter = -1;
}

void OverlayTree::StartOverlayPlacing(ScreenBase const & screen)
{
  ASSERT(IsNeedUpdate(), ());
  Clear();
  m_traits.m_modelView = screen;

#ifdef COLLECT_DISPLACEMENT_INFO
  m_displacementInfo.clear();
#endif
}

void OverlayTree::Add(ref_ptr<OverlayHandle> handle)
{
  ASSERT(IsNeedUpdate(), ());

  ScreenBase const & modelView = GetModelView();
  bool const is3dMode = modelView.isPerspective();

  handle->SetIsVisible(false);

  if (!handle->Update(modelView))
    return;

  m2::RectD const pixelRect = handle->GetExtendedPixelRect(modelView);

  if (!modelView.PixelRect().IsIntersect(handle->GetPixelRect(modelView, false)) ||
      (is3dMode && !modelView.PixelRectIn3d().IsIntersect(pixelRect)))
  {
    handle->SetIsVisible(false);
    return;
  }

  int const rank = handle->GetOverlayRank();
  ASSERT_LESS(rank, m_handles.size(), ());
  m_handles[rank].emplace_back(handle);
}

void OverlayTree::InsertHandle(ref_ptr<OverlayHandle> handle,
                               detail::OverlayInfo const & parentOverlay)
{
  ASSERT(IsNeedUpdate(), ());

  ScreenBase const & modelView = GetModelView();
  bool const is3dMode = modelView.isPerspective();

  m2::RectD const pixelRect = handle->GetExtendedPixelRect(modelView);

  TOverlayContainer elements;

  // Find elements that already on OverlayTree and it's pixel rect
  // intersect with handle pixel rect ("Intersected elements").
  ForEachInRect(pixelRect, [&] (detail::OverlayInfo const & info)
  {
    bool const isParent = (info == parentOverlay);
    if (!isParent && handle->IsIntersect(modelView, info.m_handle))
      elements.push_back(info);
  });

  if (handle->IsMinVisibilityTimeUp())
  {
    bool const boundToParent = (parentOverlay.m_handle != nullptr && handle->IsBound());

    // If handle is bound to its parent, parent's handle will be used.
    ref_ptr<OverlayHandle> handleToCompare = handle;
    if (boundToParent)
      handleToCompare = parentOverlay.m_handle;

    double const posY = handleToCompare->GetPivot(modelView, is3dMode).y;
    // In this loop we decide which element must be visible.
    // If input element "handle" more priority than all "Intersected elements"
    // than we remove all "Intersected elements" and insert input element "handle".
    // But if some of already inserted elements more priority than we don't insert "handle".
    HandleComparator comparator(m_followingMode);
    for (auto const & info : elements)
    {
      bool const pathTextComparation = handle->HasDynamicAttributes() || info.m_handle->HasDynamicAttributes();
      bool const rejectByDepth = is3dMode ? !pathTextComparation &&
                                            posY > info.m_handle->GetPivot(modelView, is3dMode).y
                                          : false;
      bool const rejectByTime = !info.m_handle->IsMinVisibilityTimeUp();
      if (rejectByDepth || rejectByTime || comparator.IsGreater(info.m_handle, handleToCompare))
      {
        // Handle is displaced and bound to its parent, parent will be displaced too.
        if (boundToParent)
        {
          Erase(parentOverlay);

        #ifdef COLLECT_DISPLACEMENT_INFO
          m_displacementInfo.emplace_back(DisplacementData(handle->GetPixelRect(modelView, is3dMode).Center(),
                                                           parentOverlay.m_handle->GetPixelRect(modelView, is3dMode).Center(),
                                                           dp::Color(0, 255, 0, 255)));
        #endif
        }

      #ifdef COLLECT_DISPLACEMENT_INFO
        m_displacementInfo.emplace_back(DisplacementData(info.m_handle->GetPixelRect(modelView, is3dMode).Center(),
                                                         handle->GetPixelRect(modelView, is3dMode).Center(),
                                                         dp::Color(0, 0, 255, 255)));
      #endif
        return;
      }
    }
  }

  // Current overlay displaces other overlay, delete them.
  for (auto const & info : elements)
    AddHandleToDelete(info);

  for (auto const & handleToDelete : m_handlesToDelete)
  {
    Erase(handleToDelete);

#ifdef COLLECT_DISPLACEMENT_INFO
  m_displacementInfo.emplace_back(DisplacementData(handle->GetPixelRect(modelView, is3dMode).Center(),
                                                   handleToDelete.m_handle->GetPixelRect(modelView, is3dMode).Center(),
                                                   dp::Color(0, 0, 255, 255)));
#endif
  }

  m_handlesToDelete.clear();

  TBase::Add(detail::OverlayInfo(handle), pixelRect);
}

void OverlayTree::EndOverlayPlacing()
{
  ASSERT(IsNeedUpdate(), ());

  HandleComparator comparator(m_followingMode);

  for (int rank = 0; rank < dp::OverlayRanksCount; rank++)
  {
    sort(m_handles[rank].begin(), m_handles[rank].end(), comparator);
    for (auto const & handle : m_handles[rank])
    {
      detail::OverlayInfo parentOverlay;
      if (!CheckHandle(handle, rank, parentOverlay))
        continue;

      InsertHandle(handle, parentOverlay);
    }

    m_handles[rank].clear();
  }

  ForEach([] (detail::OverlayInfo const & info)
  {
    info.m_handle->SetIsVisible(true);
  });

  m_frameCounter = 0;
}

bool OverlayTree::CheckHandle(ref_ptr<OverlayHandle> handle, int currentRank,
                              detail::OverlayInfo & parentOverlay) const
{
  if (currentRank == dp::OverlayRank0)
    return true;

  int const seachingRank = currentRank - 1;
  return FindNode([&](detail::OverlayInfo const & info) -> bool
  {
    if (info.m_handle->GetFeatureID() == handle->GetFeatureID() &&
        info.m_handle->GetOverlayRank() == seachingRank)
    {
      parentOverlay = info;
      return true;
    }
    return false;
  });
}

void OverlayTree::AddHandleToDelete(detail::OverlayInfo const & overlay)
{
  if (overlay.m_handle->IsBound())
  {
    ForEach([&](detail::OverlayInfo const & info)
    {
      if (info.m_handle->GetFeatureID() == overlay.m_handle->GetFeatureID())
      {
        if (find(m_handlesToDelete.begin(),
                 m_handlesToDelete.end(), info) == m_handlesToDelete.end())
          m_handlesToDelete.push_back(info);
      }
    });
  }
  else
  {
    if (find(m_handlesToDelete.begin(),
             m_handlesToDelete.end(), overlay) == m_handlesToDelete.end())
      m_handlesToDelete.push_back(overlay);
  }
}

void OverlayTree::Select(m2::PointD const & glbPoint, TSelectResult & result) const
{
  ScreenBase const & screen = m_traits.m_modelView;
  m2::PointD const pxPoint = screen.GtoP(glbPoint);

  double const kSearchRectHalfSize = 10.0;
  m2::RectD rect(pxPoint, pxPoint);
  rect.Inflate(kSearchRectHalfSize, kSearchRectHalfSize);

  ForEach([&](detail::OverlayInfo const & info)
  {
    if (rect.IsPointInside(info.m_handle->GetPivot(screen, false)))
      result.push_back(info.m_handle);
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

void OverlayTree::SetFollowingMode(bool mode)
{
  m_followingMode = mode;
}

#ifdef COLLECT_DISPLACEMENT_INFO

OverlayTree::TDisplacementInfo const & OverlayTree::GetDisplacementInfo() const
{
  return m_displacementInfo;
}

#endif

} // namespace dp
