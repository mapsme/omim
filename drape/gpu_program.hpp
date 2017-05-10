#pragma once

#include "drape/shader.hpp"
#include "drape/pointers.hpp"
#include "drape/glconstants.hpp"

#include <string>

#ifdef DEBUG
#include <memory>
#endif

namespace dp
{

class GpuProgram
{
public:
  GpuProgram(int programIndex, ref_ptr<Shader> vertexShader,
             ref_ptr<Shader> fragmentShader);
  ~GpuProgram();

  void Bind();
  void Unbind();

  int8_t GetAttributeLocation(std::string const & attributeName) const;
  int8_t GetUniformLocation(std::string const & uniformName) const;

private:
  void LoadUniformLocations();

private:
  uint32_t m_programID;

  ref_ptr<Shader> m_vertexShader;
  ref_ptr<Shader> m_fragmentShader;

  using TUniformLocations = std::map<std::string, int8_t>;
  TUniformLocations m_uniforms;

  uint8_t const m_textureSlotsCount;
};

} // namespace dp
