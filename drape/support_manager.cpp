#include "drape/support_manager.hpp"
#include "drape/glfunctions.hpp"

#include "platform/platform.hpp"

#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include "std/algorithm.hpp"
#include "std/vector.hpp"

#include "3party/Alohalytics/src/alohalytics.h"

namespace dp
{

void SupportManager::Init()
{
  string const renderer = GLFunctions::glGetString(gl_const::GLRenderer);
  string const version = GLFunctions::glGetString(gl_const::GLVersion);
  LOG(LINFO, ("Device =", GetPlatform().DeviceName(), "Renderer =", renderer, "Version =", version));
  
  // On Android support manager may be reinitialized. Here we guarantee that the info is sent once per session.
  static bool supportManagerInfoSent = false;
  if (!supportManagerInfoSent)
  {
    alohalytics::Stats::Instance().LogEvent("GPU", renderer);
    alohalytics::Stats::Instance().LogEvent("CpuCores", {{GetPlatform().DeviceName(),
                                                          strings::to_string(GetPlatform().CpuCores())}});
    supportManagerInfoSent = true;
  }

  m_isSamsungGoogleNexus = (renderer == "PowerVR SGX 540" && version.find("GOOGLENEXUS.ED945322") != string::npos);
  if (m_isSamsungGoogleNexus)
    LOG(LINFO, ("Samsung Google Nexus detected."));

  if (renderer.find("Adreno") != string::npos)
  {
    vector<string> const models = { "200", "203", "205", "220", "225" };
    for (auto const & model : models)
    {
      if (renderer.find(model) != string::npos)
      {
        LOG(LINFO, ("Adreno 200 device detected."));
        m_isAdreno200 = true;
        break;
      }
    }
  }

  m_isTegra = (renderer.find("Tegra") != string::npos);
  if (m_isTegra)
    LOG(LINFO, ("NVidia Tegra device detected."));

  m_maxLineWidth = max(1, GLFunctions::glGetMaxLineWidth());
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
