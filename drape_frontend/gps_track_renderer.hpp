#pragma once

#include "drape_frontend/gps_track_point.hpp"
#include "drape_frontend/gps_track_shape.hpp"

#include "drape/gpu_program_manager.hpp"
#include "drape/pointers.hpp"
#include "drape/uniform_values_storage.hpp"

#include "geometry/screenbase.hpp"
#include "geometry/spline.hpp"

#include <map>
#include <vector>

namespace df
{

class GpsTrackRenderer final
{
public:
  using TRenderDataRequestFn = function<void(size_t)>;
  explicit GpsTrackRenderer(TRenderDataRequestFn const & dataRequestFn);

  void AddRenderData(ref_ptr<dp::GpuProgramManager> mng,
                     drape_ptr<GpsTrackRenderData> && renderData);

  void UpdatePoints(std::vector<GpsTrackPoint> const & toAdd,
                    std::vector<uint32_t> const & toRemove);

  void RenderTrack(ScreenBase const & screen, int zoomLevel,
                   ref_ptr<dp::GpuProgramManager> mng,
                   dp::UniformValuesStorage const & commonUniforms);

  void Update();
  void Clear();
  void ClearRenderData();

private:
  float CalculateRadius(ScreenBase const & screen) const;
  size_t GetAvailablePointsCount() const;
  dp::Color CalculatePointColor(size_t pointIndex, m2::PointD const & curPoint,
                                double lengthFromStart, double fullLength) const;
  dp::Color GetColorBySpeed(double speed) const;

  TRenderDataRequestFn m_dataRequestFn;
  std::vector<drape_ptr<GpsTrackRenderData>> m_renderData;
  std::vector<GpsTrackPoint> m_points;
  m2::Spline m_pointsSpline;
  bool m_needUpdate;
  bool m_waitForRenderData;
  std::vector<pair<GpsTrackHandle*, size_t>> m_handlesCache;
  float m_radius;
  m2::PointD m_pivot;
};

} // namespace df
