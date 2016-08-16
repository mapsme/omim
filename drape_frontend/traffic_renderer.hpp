#pragma once

#include "drape/gpu_program_manager.hpp"
#include "drape/pointers.hpp"
#include "drape/uniform_values_storage.hpp"

#include "geometry/screenbase.hpp"
#include "geometry/spline.hpp"

#include "std/map.hpp"
#include "std/vector.hpp"

namespace df
{

class TrafficRenderer final
{
public:
  TrafficRenderer();

  //void AddRenderData(ref_ptr<dp::GpuProgramManager> mng,
  //                   drape_ptr<GpsTrackRenderData> && renderData);

  void RenderTraffic(ScreenBase const & screen, int zoomLevel,
                     ref_ptr<dp::GpuProgramManager> mng,
                     dp::UniformValuesStorage const & commonUniforms);

  void Clear();

private:
};

} // namespace df
