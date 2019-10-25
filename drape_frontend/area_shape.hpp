#pragma once

#include "drape_frontend/color_constants.hpp"
#include "drape_frontend/map_shape.hpp"
#include "drape_frontend/shape_view_params.hpp"

#include "drape/overlay_handle.hpp"
#include "drape/pointers.hpp"
#include "drape/utils/vertex_decl.hpp"

#include "geometry/point2d.hpp"

#include <cstdint>
#include <vector>

namespace df
{
struct BuildingOutline
{
  buffer_vector<m2::PointD, kBuildingOutlineSize> m_vertices;
  std::vector<int> m_indices;
  std::vector<m2::PointD> m_normals;
  bool m_generateOutline = false;
};

class Area3dShapeHandle : public dp::OverlayHandle
{
  using TBase = dp::OverlayHandle;

public:
  Area3dShapeHandle(FeatureID const & featureId, uint32_t verticesCount,
                    PaletteColor color, ref_ptr<dp::TextureManager> textureManager);
  void GetAttributeMutation(ref_ptr<dp::AttributeBufferMutator> mutator) const override;
  bool Update(ScreenBase const & screen) override;
  m2::RectD GetPixelRect(ScreenBase const & screen, bool perspective) const override;
  void GetPixelShape(ScreenBase const & screen, bool perspective, Rects & rects) const override;
  bool IndexesRequired() const override;

  void SetHighlightColor(PaletteColor color, ref_ptr<dp::TextureManager> textureManager);

private:
  uint32_t const m_verticesCount;
  gpu::Area3dDynamicVertex::TTexCoord m_texCoord;
  mutable bool m_needUpdate;
};

class AreaShape : public MapShape
{
public:
  AreaShape(std::vector<m2::PointD> && triangleList, BuildingOutline && buildingOutline,
            AreaViewParams const & params);

  void Draw(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::Batcher> batcher,
            ref_ptr<dp::TextureManager> textures) const override;

private:
  void DrawArea(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::Batcher> batcher,
                m2::PointD const & colorUv, m2::PointD const & outlineUv,
                ref_ptr<dp::Texture> texture) const;
  void DrawArea3D(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::Batcher> batcher,
                  m2::PointD const & colorUv, m2::PointD const & outlineUv,
                  ref_ptr<dp::TextureManager> textureManager, ref_ptr<dp::Texture> texture) const;
  void DrawHatchingArea(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::Batcher> batcher,
                        m2::PointD const & colorUv, ref_ptr<dp::Texture> texture,
                        ref_ptr<dp::Texture> hatchingTexture) const;

  std::vector<m2::PointD> m_vertexes;
  BuildingOutline m_buildingOutline;
  AreaViewParams m_params;
};
}  // namespace df
