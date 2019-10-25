#pragma once

#include "drape/color.hpp"

#include <string>

namespace dp
{
class ColorGetter
{
public:
  virtual ~ColorGetter() = default;
  virtual dp::Color GetColor(std::string const & colorName) = 0;
};
}  // namespace dp
