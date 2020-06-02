#pragma once

#include "drape_frontend/map_shape.hpp"
#include "drape_frontend/render_state_extension.hpp"
#include "drape_frontend/shape_view_params.hpp"

#include "drape/binding_info.hpp"

#include "geometry/spline.hpp"

#include <memory>

namespace df
{
class LineShapeInfo;

class LineShape : public MapShape
{
public:
  LineShape(m2::SharedSpline const & spline, LineViewParams const & params, bool showArrows = false);
  ~LineShape() override;

  void Prepare(ref_ptr<dp::TextureManager> textures) const override;
  void Draw(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::Batcher> batcher,
            ref_ptr<dp::TextureManager> textures) const override;

private:
  template <typename TBuilder>
  void Construct(TBuilder & builder) const;

  bool CanBeSimplified(int & lineWidth) const;

  bool const m_showArrows;

  LineViewParams m_params;
  m2::SharedSpline m_spline;
  mutable std::unique_ptr<LineShapeInfo> m_lineShapeInfo;
  mutable bool m_isSimple;
};
}  // namespace df

