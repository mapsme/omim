#pragma once

#include "../drape/vertex_array_buffer.hpp"
#include "../drape/glstate.hpp"
#include "../drape/gpu_program_manager.hpp"
#include "../drape/texture_manager.hpp"
#include "../drape/uniform_values_storage.hpp"

#include "../geometry/screenbase.hpp"

namespace df
{

class MyPosition
{
public:
  MyPosition(dp::RefPointer<dp::TextureManager> mng);

  ///@param pt = mercator point
  void SetPosition(m2::PointF const & pt);
  void SetAzimut(float azimut);
  void SetAccuracy(float accuracy);
  void SetIsVisible(bool isVisible);

  void Render(ScreenBase const & screen,
              dp::RefPointer<dp::GpuProgramManager> mng,
              dp::UniformValuesStorage const & commonUniforms);

private:
  void CacheAccuracySector(dp::RefPointer<dp::TextureManager> mng);
  void CachePointPosition(dp::RefPointer<dp::TextureManager> mng);

private:
  struct RenderNode
  {
    RenderNode(dp::GLState const & state, dp::TransferPointer<dp::VertexArrayBuffer> buffer)
      : m_state(state)
      , m_buffer(buffer)
      , m_isBuilded(false)
    {
    }

    dp::GLState m_state;
    dp::MasterPointer<dp::VertexArrayBuffer> m_buffer;
    bool m_isBuilded;
  };

  enum EMyPositionPart
  {
    // don't change order and values
    MY_POSITION_ACCURACY = 0,
    MY_POSITION_ARROW = 1,
    MY_POSITION_POINT = 2
  };

  void RenderPart(dp::RefPointer<dp::GpuProgramManager> mng,
                  dp::UniformValuesStorage const & uniforms,
                  EMyPositionPart part);

private:
  m2::PointF m_position;
  float m_azimut;
  float m_accuracy;
  bool m_showAzimut;
  bool m_isVisible;

  using TPart = pair<dp::IndicesRange, size_t>;

  vector<TPart> m_parts;
  vector<RenderNode> m_nodes;
};

}
