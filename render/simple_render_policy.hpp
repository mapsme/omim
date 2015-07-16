#pragma once

#include "render_policy.hpp"

namespace rg
{

class SimpleRenderPolicy : public RenderPolicy
{
private:
  shared_ptr<graphics::Overlay> m_overlay;

public:
  SimpleRenderPolicy(Params const & p);

  void DrawFrame(shared_ptr<PaintEvent> const & paintEvent,
                 ScreenBase const & screenBase);
  graphics::Overlay * FrameOverlay() const;
};

} // namespace rg
