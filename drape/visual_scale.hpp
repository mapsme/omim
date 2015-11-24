#pragma once

#include "platform/platform.hpp"

namespace dp
{
inline double VisualScale(double exactDensityDPI) noexcept
{
  double constexpr kMdpiDensityDPI = 160.;
  // In case of tablets and iPads increased DPI is used to make visual scale bigger.
  // According to designer decision visual scale on tablets should be kTabletFactor bigger
  // than on phones.
  // @TODO. There's a better idea. The visual scale should be calculated based on dpi.
  // Then it should be corrected based on the length of screen diagonal.
  double constexpr kTabletFactor = 1.28;

  if (GetPlatform().IsTablet())
    exactDensityDPI *= kTabletFactor;
  // For some old devices (for example iPad 2) the density could be less than 160 DPI.
  // Returns one in that case to keep readable text on the map.
  return max(1., exactDensityDPI / kMdpiDensityDPI);
}
} //  namespace dp
