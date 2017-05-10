#pragma once

#include "drape/glfunctions.hpp"

#include <string>
#include <cstdint>

namespace dp
{

class Shader
{
public:
  enum Type
  {
    VertexShader,
    FragmentShader
  };

  Shader(std::string const & shaderSource, std::string const & defines, Type type);
  ~Shader();

  int GetID() const;

private:
  uint32_t m_glID;
};

void PreprocessShaderSource(std::string & src);

} // namespace dp
