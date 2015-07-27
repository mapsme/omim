#pragma once

#include "navigator.hpp"

#include "render/scales_processor.hpp"

#include "geometry/any_rect2d.hpp"

namespace rg
{

void CheckMinGlobalRect(m2::RectD & rect, ScalesProcessor const & sp);
m2::AnyRectD ToRotated(m2::RectD const & rect, Navigator const & navigator);
void SetRectFixedAR(m2::AnyRectD const & rect, Navigator & navigator);

} // namespace rg
