#pragma once

#include "drape/shader.hpp"
#include "drape/pointers.hpp"
#include "drape/glconstants.hpp"

#include "std/string.hpp"

#ifdef DEBUG
  #include "std/unique_ptr.hpp"
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

  int8_t GetAttributeLocation(string const & attributeName) const;
  int8_t GetUniformLocation(string const & uniformName) const;

private:
  void LoadUniformLocations();

private:
  uint32_t m_programID;

  ref_ptr<Shader> m_vertexShader;
  ref_ptr<Shader> m_fragmentShader;

  using TUniformLocations = map<string, int8_t>;
  TUniformLocations m_uniforms;

  uint8_t const m_textureSlotsCount;
};

} // namespace dp
