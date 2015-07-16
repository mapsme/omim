#pragma once

#include "basic_tiling_render_policy.hpp"

namespace graphics
{

namespace gl
{

class RenderContext;

} // namespace gl

class ResourceManager;

} // namespace graphics

namespace rg
{

class WindowHandle;

class TilingRenderPolicyMT : public BasicTilingRenderPolicy
{
public:

  TilingRenderPolicyMT(Params const & p);

  ~TilingRenderPolicyMT();

  void SetRenderFn(TRenderFn const & renderFn);
};

} // namespace rg
