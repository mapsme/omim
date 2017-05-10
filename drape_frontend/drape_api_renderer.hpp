#pragma once

#include "drape_frontend/drape_api_builder.hpp"

#include "drape/gpu_program_manager.hpp"

#include "geometry/screenbase.hpp"

#include <vector>
#include <string>

namespace df
{

class DrapeApiRenderer
{
public:
  DrapeApiRenderer() = default;

  void AddRenderProperties(ref_ptr<dp::GpuProgramManager> mng,
                           std::vector<drape_ptr<DrapeApiRenderProperty>> && properties);

  void RemoveRenderProperty(std::string const & id);
  void Clear();

  void Render(ScreenBase const & screen, ref_ptr<dp::GpuProgramManager> mng,
              dp::UniformValuesStorage const & commonUniforms);

private:
  std::vector<drape_ptr<DrapeApiRenderProperty>> m_properties;
};

} // namespace df
