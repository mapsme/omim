#pragma once

#include "drape/drape_diagnostics.hpp"
#include "drape/overlay_handle.hpp"

#include "geometry/screenbase.hpp"
#include "geometry/tree4d.hpp"

#include "base/buffer_vector.hpp"

#include <array>
#include <vector>
#include <unordered_set>

namespace dp
{

namespace detail
{

struct OverlayTraits
{
  ScreenBase m_modelView;

  inline m2::RectD const LimitRect(ref_ptr<OverlayHandle> const & handle)
  {
    return handle->GetExtendedPixelRect(m_modelView);
  }
};

struct OverlayHasher
{
  hash<OverlayHandle*> m_hasher;

  size_t operator()(ref_ptr<OverlayHandle> const & handle) const
  {
    return m_hasher(handle.get());
  }
};

}

using TOverlayContainer = buffer_vector<ref_ptr<OverlayHandle>, 8>;

class OverlayTree : public m4::Tree<ref_ptr<OverlayHandle>, detail::OverlayTraits>
{
  using TBase = m4::Tree<ref_ptr<OverlayHandle>, detail::OverlayTraits>;

public:
  using HandlesCache = std::unordered_set<ref_ptr<OverlayHandle>, detail::OverlayHasher>;

  OverlayTree();

  bool Frame();
  bool IsNeedUpdate() const;

  void StartOverlayPlacing(ScreenBase const & screen);
  void Add(ref_ptr<OverlayHandle> handle);
  void Remove(ref_ptr<OverlayHandle> handle);
  void EndOverlayPlacing();

  HandlesCache const & GetHandlesCache() const { return m_handlesCache; }

  void Select(m2::RectD const & rect, TOverlayContainer & result) const;
  void Select(m2::PointD const & glbPoint, TOverlayContainer & result) const;

  void SetFollowingMode(bool mode);
  void SetDisplacementEnabled(bool enabled);
  void SetDisplacementMode(int displacementMode);

  void SetSelectedFeature(FeatureID const & featureID);
  bool GetSelectedFeatureRect(ScreenBase const & screen, m2::RectD & featureRect);

#ifdef COLLECT_DISPLACEMENT_INFO
  struct DisplacementData
  {
    m2::PointF m_arrowStart;
    m2::PointF m_arrowEnd;
    dp::Color m_arrowColor;
    DisplacementData(m2::PointF const & arrowStart, m2::PointF const & arrowEnd, dp::Color const & arrowColor)
      : m_arrowStart(arrowStart), m_arrowEnd(arrowEnd), m_arrowColor(arrowColor)
    {}
  };
  using TDisplacementInfo = std::vector<DisplacementData>;
  TDisplacementInfo const & GetDisplacementInfo() const;
#endif

private:
  ScreenBase const & GetModelView() const { return m_traits.m_modelView; }
  void InsertHandle(ref_ptr<OverlayHandle> handle, int currentRank,
                    ref_ptr<OverlayHandle> const & parentOverlay);
  bool CheckHandle(ref_ptr<OverlayHandle> handle, int currentRank,
                   ref_ptr<OverlayHandle> & parentOverlay) const;
  void DeleteHandle(ref_ptr<OverlayHandle> const & handle);

  ref_ptr<OverlayHandle> FindParent(ref_ptr<OverlayHandle> handle, int searchingRank) const;
  void DeleteHandleWithParents(ref_ptr<OverlayHandle> handle, int currentRank);

  int m_frameCounter;
  std::array<std::vector<ref_ptr<OverlayHandle>>, dp::OverlayRanksCount> m_handles;
  HandlesCache m_handlesCache;
  bool m_followingMode;

  bool m_isDisplacementEnabled;
  int m_displacementMode;

  FeatureID m_selectedFeatureID;

#ifdef COLLECT_DISPLACEMENT_INFO
  TDisplacementInfo m_displacementInfo;
#endif
};

} // namespace dp
