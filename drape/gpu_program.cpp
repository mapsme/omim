#include "drape/gpu_program.hpp"
#include "drape/glfunctions.hpp"
#include "drape/glstate.hpp"
#include "drape/support_manager.hpp"

#include "base/logging.hpp"

namespace dp
{
GpuProgram::GpuProgram(std::string const & programName,
                       ref_ptr<Shader> vertexShader, ref_ptr<Shader> fragmentShader,
                       uint8_t textureSlotsCount)
  : m_programName(programName)
  , m_vertexShader(vertexShader)
  , m_fragmentShader(fragmentShader)
  , m_textureSlotsCount(textureSlotsCount)
{
  m_programID = GLFunctions::glCreateProgram();
  GLFunctions::glAttachShader(m_programID, m_vertexShader->GetID());
  GLFunctions::glAttachShader(m_programID, m_fragmentShader->GetID());

  string errorLog;
  if (!GLFunctions::glLinkProgram(m_programID, errorLog))
    LOG(LERROR, ("Program ", programName, " link error = ", errorLog));

  // On Tegra3 glGetActiveUniform isn't work if you detach shaders after linking.
  LoadUniformLocations();

  // On Tegra2 we cannot detach shaders at all.
  // https://devtalk.nvidia.com/default/topic/528941/alpha-blending-not-working-on-t20-and-t30-under-ice-cream-sandwich/
  if (!SupportManager::Instance().IsTegraDevice())
  {
    GLFunctions::glDetachShader(m_programID, m_vertexShader->GetID());
    GLFunctions::glDetachShader(m_programID, m_fragmentShader->GetID());
  }
}

GpuProgram::~GpuProgram()
{
  Unbind();

  if (SupportManager::Instance().IsTegraDevice())
  {
    GLFunctions::glDetachShader(m_programID, m_vertexShader->GetID());
    GLFunctions::glDetachShader(m_programID, m_fragmentShader->GetID());
  }

  GLFunctions::glDeleteProgram(m_programID);
}

void GpuProgram::Bind()
{
  // Deactivate all unused textures.
  uint8_t const usedSlots = TextureState::GetLastUsedSlots();
  for (uint8_t i = m_textureSlotsCount; i < usedSlots; i++)
  {
    GLFunctions::glActiveTexture(gl_const::GLTexture0 + i);
    GLFunctions::glBindTexture(0);
  }

  GLFunctions::glUseProgram(m_programID);
}

void GpuProgram::Unbind()
{
  GLFunctions::glUseProgram(0);
}

int8_t GpuProgram::GetAttributeLocation(std::string const & attributeName) const
{
  return GLFunctions::glGetAttribLocation(m_programID, attributeName);
}

int8_t GpuProgram::GetUniformLocation(std::string const & uniformName) const
{
  auto const it = m_uniforms.find(uniformName);
  if (it == m_uniforms.end())
    return -1;

  return it->second.m_location;
}

glConst GpuProgram::GetUniformType(std::string const & uniformName) const
{
  auto const it = m_uniforms.find(uniformName);
  if (it == m_uniforms.end())
    return -1;

  return it->second.m_type;
}

GpuProgram::UniformsInfo const & GpuProgram::GetUniformsInfo() const
{
  return m_uniforms;
}

void GpuProgram::LoadUniformLocations()
{
  auto const uniformsCount = GLFunctions::glGetProgramiv(m_programID, gl_const::GLActiveUniforms);
  for (int i = 0; i < uniformsCount; ++i)
  {
    int32_t size = 0;
    UniformInfo info;
    std::string name;
    GLFunctions::glGetActiveUniform(m_programID, static_cast<uint32_t>(i), &size, &info.m_type, name);
    if (info.m_type != gl_const::GLFloatType && info.m_type != gl_const::GLFloatVec2 &&
        info.m_type != gl_const::GLFloatVec3 && info.m_type != gl_const::GLFloatVec4 &&
        info.m_type != gl_const::GLFloatMat4 && info.m_type != gl_const::GLSampler2D)
    {
      CHECK(false, ("Used uniform has unsupported type. Program =", m_programName, "Type =", info.m_type));
    }

    info.m_location = GLFunctions::glGetUniformLocation(m_programID, name);
    m_uniforms[name] = std::move(info);
  }
  m_numericUniformsCount = CalculateNumericUniformsCount();
}

uint32_t GpuProgram::CalculateNumericUniformsCount() const
{
  uint32_t counter = 0;
  for (auto const & u : m_uniforms)
  {
    if (u.second.m_type != gl_const::GLSampler2D)
      counter++;
  }
  return counter;
}
}  // namespace dp
