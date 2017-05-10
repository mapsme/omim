#include "drape/shader.hpp"
#include "drape/shader_def.hpp"

#include "base/assert.hpp"
#include "base/string_utils.hpp"

#include <algorithm>

namespace dp
{

using strings::to_string;

namespace
{

glConst convert(Shader::Type t)
{
  if (t == Shader::VertexShader)
    return gl_const::GLVertexShader;

  return gl_const::GLFragmentShader;
}

void ResolveGetTexel(std::string & result, std::string const & sampler, int count)
{
  std::string const index = "index";
  std::string const answer = "answer";
  std::string const texIndex = "texIndex";
  std::string const texCoord = "texCoord";

  result.reserve(250 + 130 * count);

  for (int i = 0; i < count; ++i)
  {
    std::string const number = to_string(i);
    result.append("const int ").append(index).append(number).append(" = ").append(number).append(";\n");
  }

  result.append("uniform sampler2D u_textures[").append(to_string(count)).append("];\n");

  // Function signature
  result.append(LOW_P).append(" vec4 getTexel(int ").append(texIndex).append(", ")
        .append(MAXPREC_P).append(" vec2 ").append(texCoord).append("){ \n");

  // Declare result var;
  result.append(LOW_P).append(" vec4 ").append(answer).append(";\n");

  for (int i = 0; i < count; ++i)
  {
    std::string constIndex = index + to_string(i);
    if (i != 0)
      result.append("else ");
    result.append("if (").append(texIndex).append("==").append(constIndex).append(")\n");
    result.append(answer).append("=texture2D(")
          .append(sampler).append("[").append(constIndex).append("],")
          .append(texCoord).append(");\n");
  }
  result.append("return ").append(answer).append(";}");
}

}

void PreprocessShaderSource(std::string & src)
{
  std::string const replacement("~getTexel~");
  auto const pos = src.find(replacement);
  if (pos == std::string::npos)
    return;
  std::string injector = "";
  int const count = std::min(8, GLFunctions::glGetInteger(gl_const::GLMaxFragmentTextures));
  ResolveGetTexel(injector, "u_textures", count);
  src.replace(pos, replacement.length(), injector);
}

Shader::Shader(std::string const & shaderSource, std::string const & defines, Type type)
  : m_glID(0)
{
  m_glID = GLFunctions::glCreateShader(convert(type));
  std::string source = shaderSource;
  PreprocessShaderSource(source);
  GLFunctions::glShaderSource(m_glID, source, defines);
  std::string errorLog;
  bool result = GLFunctions::glCompileShader(m_glID, errorLog);
  CHECK(result, ("Shader compile error : ", errorLog));
}

Shader::~Shader()
{
  GLFunctions::glDeleteShader(m_glID);
}

int Shader::GetID() const
{
  return m_glID;
}

} // namespace dp
