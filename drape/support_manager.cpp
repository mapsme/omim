#include "drape/support_manager.hpp"
#include "drape/glfunctions.hpp"

#include "base/logging.hpp"

#include <algorithm>
#include <vector>

#include "3party/Alohalytics/src/alohalytics.h"

namespace dp
{

void SupportManager::Init()
{
  std::string const renderer = GLFunctions::glGetString(gl_const::GLRenderer);
  std::string const version = GLFunctions::glGetString(gl_const::GLVersion);
  LOG(LINFO, ("Renderer =", renderer, "Version =", version));

  // On Android the engine may be recreated. Here we guarantee that GPU info is sent once per session.
  static bool gpuInfoSent = false;
  if (!gpuInfoSent)
  {
    alohalytics::Stats::Instance().LogEvent("GPU", renderer);
    gpuInfoSent = true;
  }

  m_isSamsungGoogleNexus = (renderer == "PowerVR SGX 540" && version.find("GOOGLENEXUS.ED945322") != std::string::npos);
  if (m_isSamsungGoogleNexus)
    LOG(LINFO, ("Samsung Google Nexus detected."));

  if (renderer.find("Adreno") != std::string::npos)
  {
    std::vector<std::string> const models = { "200", "203", "205", "220", "225" };
    for (auto const & model : models)
    {
      if (renderer.find(model) != std::string::npos)
      {
        LOG(LINFO, ("Adreno 200 device detected."));
        m_isAdreno200 = true;
        break;
      }
    }
  }

  m_isTegra = (renderer.find("Tegra") != std::string::npos);
  if (m_isTegra)
    LOG(LINFO, ("NVidia Tegra device detected."));

  m_maxLineWidth = std::max(1, GLFunctions::glGetMaxLineWidth());
  LOG(LINFO, ("Max line width =", m_maxLineWidth));
}

bool SupportManager::IsSamsungGoogleNexus() const
{
  return m_isSamsungGoogleNexus;
}

bool SupportManager::IsAdreno200Device() const
{
  return m_isAdreno200;
}

bool SupportManager::IsTegraDevice() const
{
  return m_isTegra;
}

int SupportManager::GetMaxLineWidth() const
{
  return m_maxLineWidth;
}

SupportManager & SupportManager::Instance()
{
  static SupportManager manager;
  return manager;
}

} // namespace dp
