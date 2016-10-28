#include "drape/glstate.hpp"
#include "drape/glfunctions.hpp"

#include "base/buffer_vector.hpp"
#include "std/bind.hpp"

#define TEXTURE_BIT 0x1

namespace dp
{

BlendingParams::BlendingParams()
  : m_blendFunction(gl_const::GLAddBlend)
  , m_blendSrcFactor(gl_const::GLSrcAlfa)
  , m_blendDstFactor(gl_const::GLOneMinusSrcAlfa)
{
}

void BlendingParams::Apply() const
{
  GLFunctions::glBlendEquation(m_blendFunction);
  GLFunctions::glBlendFunc(m_blendSrcFactor, m_blendDstFactor);
}

Blending::Blending(bool isEnabled)
  : m_isEnabled(isEnabled)
{}

void Blending::Apply() const
{
  if (m_isEnabled)
    GLFunctions::glEnable(gl_const::GLBlending);
  else
    GLFunctions::glDisable(gl_const::GLBlending);
}

bool Blending::operator < (Blending const & other) const
{
  return m_isEnabled < other.m_isEnabled;
}

bool Blending::operator == (Blending const & other) const
{
  return m_isEnabled == other.m_isEnabled;
}

GLState::GLState(uint32_t gpuProgramIndex, DepthLayer depthLayer)
  : m_gpuProgramIndex(gpuProgramIndex)
  , m_gpuProgram3dIndex(gpuProgramIndex)
  , m_depthLayer(depthLayer)
  , m_depthFunction(gl_const::GLLessOrEqual)
  , m_textureFilter(gl_const::GLLinear)
  , m_colorTexture(nullptr)
  , m_maskTexture(nullptr)
  , m_drawAsLine(false)
  , m_lineWidth(1)
{}

glConst GLState::GetDepthFunction() const
{
  return m_depthFunction;
}

void GLState::SetDepthFunction(glConst functionName)
{
  m_depthFunction = functionName;
}

glConst GLState::GetTextureFilter() const
{
  return m_textureFilter;
}

void GLState::SetTextureFilter(glConst filter)
{
  m_textureFilter = filter;
}

bool GLState::GetDrawAsLine() const
{
  return m_drawAsLine;
}

void GLState::SetDrawAsLine(bool drawAsLine)
{
  m_drawAsLine = drawAsLine;
}

int GLState::GetLineWidth() const
{
  return m_lineWidth;
}

void GLState::SetLineWidth(int width)
{
  m_lineWidth = width;
}

bool GLState::operator<(GLState const & other) const
{
  if (m_depthLayer != other.m_depthLayer)
    return m_depthLayer < other.m_depthLayer;
  if (!(m_blending == other.m_blending))
    return m_blending < other.m_blending;
  if (m_gpuProgramIndex != other.m_gpuProgramIndex)
    return m_gpuProgramIndex < other.m_gpuProgramIndex;
  if (m_gpuProgram3dIndex != other.m_gpuProgram3dIndex)
    return m_gpuProgram3dIndex < other.m_gpuProgram3dIndex;
  if (m_depthFunction != other.m_depthFunction)
    return m_depthFunction < other.m_depthFunction;
  if (m_colorTexture != other.m_colorTexture)
    return m_colorTexture < other.m_colorTexture;
  if (m_maskTexture != other.m_maskTexture)
    return m_maskTexture < other.m_maskTexture;
  if (m_textureFilter != other.m_textureFilter)
    return m_textureFilter < other.m_textureFilter;
  if (m_drawAsLine != other.m_drawAsLine)
    return m_drawAsLine < other.m_drawAsLine;

  return m_lineWidth < other.m_lineWidth;
}

bool GLState::operator==(GLState const & other) const
{
  return m_depthLayer == other.m_depthLayer &&
         m_gpuProgramIndex == other.m_gpuProgramIndex &&
         m_gpuProgram3dIndex == other.m_gpuProgram3dIndex &&
         m_blending == other.m_blending &&
         m_colorTexture == other.m_colorTexture &&
         m_maskTexture == other.m_maskTexture &&
         m_textureFilter == other.m_textureFilter &&
         m_depthFunction == other.m_depthFunction &&
         m_drawAsLine == other.m_drawAsLine &&
         m_lineWidth == other.m_lineWidth;
}

bool GLState::operator!=(GLState const & other) const
{
  return !operator==(other);
}

namespace
{

struct UniformApplyer
{
  ref_ptr<GpuProgram> m_program;

  void operator()(UniformValue const & value) const
  {
    ASSERT(m_program != nullptr, ());
    value.Apply(m_program);
  }
};

} // namespace

uint8_t TextureState::m_usedSlots = 0;

void TextureState::ApplyTextures(GLState state, ref_ptr<GpuProgram> program)
{
  m_usedSlots = 0;

  ref_ptr<Texture> tex = state.GetColorTexture();
  int8_t colorTexLoc = -1;
  if (tex != nullptr && (colorTexLoc = program->GetUniformLocation("u_colorTex")) >= 0)
  {
    GLFunctions::glActiveTexture(gl_const::GLTexture0);
    tex->Bind();
    GLFunctions::glUniformValuei(colorTexLoc, 0);
    tex->SetFilter(state.GetTextureFilter());
    m_usedSlots++;
  }

  tex = state.GetMaskTexture();
  int8_t maskTexLoc = -1;
  if (tex != nullptr && (maskTexLoc = program->GetUniformLocation("u_maskTex")) >= 0)
  {
    GLFunctions::glActiveTexture(gl_const::GLTexture0 + 1);
    tex->Bind();
    GLFunctions::glUniformValuei(maskTexLoc, 1);
    tex->SetFilter(state.GetTextureFilter());
    m_usedSlots++;
  }
}

uint8_t TextureState::GetLastUsedSlots()
{
  return m_usedSlots;
}

void ApplyUniforms(UniformValuesStorage const & uniforms, ref_ptr<GpuProgram> program)
{
  static UniformApplyer applyer;
  applyer.m_program = program;
  uniforms.ForeachValue(applyer);
  applyer.m_program = nullptr;
}

void ApplyBlending(GLState state, ref_ptr<GpuProgram> program)
{
  state.GetBlending().Apply();
}

void ApplyState(GLState state, ref_ptr<GpuProgram> program)
{
  TextureState::ApplyTextures(state, program);
  ApplyBlending(state, program);
  GLFunctions::glDepthFunc(state.GetDepthFunction());
  GLFunctions::glLineWidth(state.GetLineWidth());
}

}
