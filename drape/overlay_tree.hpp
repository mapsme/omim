#pragma once

#include "drape/overlay_handle.hpp"

#include "geometry/screenbase.hpp"
#include "geometry/tree4d.hpp"


namespace dp
{

namespace detail
{

struct OverlayInfo
{
  ref_ptr<OverlayHandle> m_handle;
  bool m_isTransparent = false;

  OverlayInfo() = default;
  OverlayInfo(ref_ptr<OverlayHandle> handle, bool isTransparent)
    : m_handle(handle)
    , m_isTransparent(isTransparent)
  {}

  bool operator==(OverlayInfo const & rhs) const
  {
    return m_handle == rhs.m_handle && m_isTransparent == rhs.m_isTransparent;
  }
};

struct OverlayTraits
{
  ScreenBase m_modelView;

  inline m2::RectD const LimitRect(OverlayInfo const & info)
  {
    return info.m_handle->GetPixelRect(m_modelView);
  }
};

}

class OverlayTree : public m4::Tree<detail::OverlayInfo, detail::OverlayTraits>
{
  typedef m4::Tree<detail::OverlayInfo, detail::OverlayTraits> BaseT;

public:
  void StartOverlayPlacing(ScreenBase const & screen, bool canOverlap = false);
  void Add(ref_ptr<OverlayHandle> handle, bool isTransparent);
  void EndOverlayPlacing();

private:
  ScreenBase const & GetModelView() const { return m_traits.m_modelView; }

private:
  bool m_canOverlap;
};

} // namespace dp
