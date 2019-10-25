#pragma once

#include "drape_frontend/animation/show_hide_animation.hpp"
#include "drape_frontend/animation/value_mapping.hpp"
#include "drape_frontend/frame_values.hpp"
#include "drape_frontend/render_node.hpp"

#include "drape/graphics_context.hpp"

#include "geometry/point2d.hpp"
#include "geometry/screenbase.hpp"

#include "indexer/feature.hpp"

#include <set>
#include <vector>

namespace dp
{
class TextureManager;
}  // namespace dp

namespace gpu
{
class ProgramManager;
}  // namespace gpu

namespace df
{
class SelectionShape
{
public:
  enum ESelectedObject
  {
    OBJECT_EMPTY,
    OBJECT_POI,
    OBJECT_USER_MARK,
    OBJECT_MY_POSITION
  };

  SelectionShape(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::TextureManager> mng);

  void SetPosition(m2::PointD const & position) { m_position = position; }
  void Show(ESelectedObject obj, FeatureID const & fid, m2::PointD const & position, double positionZ,
            bool isAnimate);
  void Hide();
  void Render(ref_ptr<dp::GraphicsContext> context, ref_ptr<gpu::ProgramManager> mng, ScreenBase const & screen,
              int zoomLevel, FrameValues const & frameValues);

  bool IsVisible(ScreenBase const & screen, m2::PointD & pxPos) const;
  double GetRadius() const { return m_radius; }
  double GetPositionZ() const { return m_positionZ; }

  ESelectedObject GetSelectedObject() const;

  void AddSelectionGeometry(drape_ptr<RenderNode> && renderNode, int recacheId);
  int GetRecacheId() const { return m_recacheId; }
  m2::RectD GetSelectionGeometryBoundingBox() const;
  bool HasSelectionGeometry() const { return !m_selectionGeometry.empty(); }

  bool IsComplexSelection() const;
  void AddSelectedFeature(FeatureID const & featureId);
  void ClearSelectedFeatures();
  std::set<FeatureID> const & GetSelectedFeatures() const;

private:
  m2::PointD m_position;
  double m_positionZ;
  double m_radius;
  ShowHideAnimation m_animation;
  ESelectedObject m_selectedObject;

  drape_ptr<RenderNode> m_renderNode;
  ValueMapping<float> m_mapping;

  std::vector<drape_ptr<RenderNode>> m_selectionGeometry;
  int m_recacheId = -1;

  std::set<FeatureID> m_selectedFeatures;
};
}  // namespace df
