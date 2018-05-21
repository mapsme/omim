#pragma once

#include "drape_frontend/gui/shape.hpp"

namespace gui
{
class Compass : public Shape
{
public:
  Compass(gui::Position const & position) : Shape(position) {}

  drape_ptr<ShapeRenderer> Draw(m2::PointF & compassSize, ref_ptr<dp::TextureManager> tex,
                                TTapHandler const & tapHandler) const;
};
}  // namespace gui
