#pragma once

#include "basic_tiling_render_policy.hpp"

namespace rg
{

class TilingRenderPolicyST : public BasicTilingRenderPolicy
{
public:

  TilingRenderPolicyST(Params const & p);

  ~TilingRenderPolicyST();

  void SetRenderFn(TRenderFn const & renderFn);
};

} // namespace rg
