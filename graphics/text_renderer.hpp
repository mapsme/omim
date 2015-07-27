#pragma once

#include "graphics/image_renderer.hpp"
#include "graphics/defines.hpp"
#include "graphics/font_desc.hpp"
#include "graphics/text_element.hpp"

#include "geometry/tree4d.hpp"

#include "std/shared_ptr.hpp"


namespace graphics
{
  struct Glyph;

  class TextRenderer : public ImageRenderer
  {
  public:

    typedef ImageRenderer base_t;

    TextRenderer(base_t::Params const & params);

    void drawStraightGlyph(m2::PointD const & ptOrg,
                           m2::PointD const & ptGlyph,
                           Glyph const * p,
                           float depth);

    void drawGlyph(m2::PointD const & ptOrg,
                   m2::PointD const & ptGlyph,
                   ang::AngleD const & angle,
                   float blOffset,
                   Glyph const * p,
                   double depth);


    GlyphCache * glyphCache() const;
  };
}
