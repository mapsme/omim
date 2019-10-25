#pragma once

#include "drape/color.hpp"
#include "drape/color_getter.hpp"

#include <string>

namespace df
{
enum class PaletteColor : uint8_t
{
  NoSelection = 0,
  Selection,

  Count
};

using ColorConstant = std::string;

dp::Color GetColorConstant(ColorConstant const & constant);

void LoadTransitColors();

ColorConstant GetTransitColorName(ColorConstant const & localName);
ColorConstant GetTransitTextColorName(ColorConstant const & localName);
ColorConstant GetPaletteColorName(PaletteColor c);

class ColorGetter : public dp::ColorGetter
{
public:
  dp::Color GetColor(std::string const & colorName) override
  {
    if (colorName.empty())
      return dp::Color::Transparent();
    return GetColorConstant(colorName);
  }
};
} //  namespace df
