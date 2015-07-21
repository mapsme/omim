#include "drape/texture.hpp"

#include "drape/glfunctions.hpp"
#include "drape/glextensions_list.hpp"
#include "drape/utils/gpu_mem_tracker.hpp"

#include "base/math.hpp"

#define ASSERT_ID ASSERT(GetID() != -1, ())

namespace dp
{

Texture::ResourceInfo::ResourceInfo(m2::RectF const & texRect)
  : m_texRect(texRect) {}

m2::RectF const & Texture::ResourceInfo::GetTexRect() const
{
  return m_texRect;
}

//////////////////////////////////////////////////////////////////

Texture::Texture()
  : m_usePixelBuffer(false)
  , m_textureID(-1)
  , m_width(0)
  , m_height(0)
  , m_format(dp::UNSPECIFIED)
  , m_pixelBufferID(-1)
  , m_pixelBufferSize(0)
{
}

Texture::~Texture()
{
  Destroy();
}

void Texture::Create(uint32_t width, uint32_t height, TextureFormat format)
{
  Create(width, height, format, nullptr);
}

void Texture::Create(uint32_t width, uint32_t height, TextureFormat format, ref_ptr<void> data)
{
  m_format = format;
  m_width = width;
  m_height = height;
  if (!GLExtensionsList::Instance().IsSupported(GLExtensionsList::TextureNPOT))
  {
    m_width = my::NextPowOf2(width);
    m_height = my::NextPowOf2(height);
  }

  m_textureID = GLFunctions::glGenTexture();
  GLFunctions::glBindTexture(m_textureID);

  glConst layout;
  glConst pixelType;
  uint32_t channelCount;
  UnpackFormat(format, layout, pixelType, channelCount);

  GLFunctions::glTexImage2D(m_width, m_height, layout, pixelType, data.get());
  SetFilterParams(gl_const::GLLinear, gl_const::GLLinear);
  SetWrapMode(gl_const::GLClampToEdge, gl_const::GLClampToEdge);

  if (m_usePixelBuffer)
  {
    float const pboPercent = 0.1f;
    m_pixelBufferSize = static_cast<uint32_t>(pboPercent * m_width * m_height * channelCount);
    m_pixelBufferID = GLFunctions::glGenBuffer();
    GLFunctions::glBindBuffer(m_pixelBufferID, gl_const::GLPixelBufferWrite);
    GLFunctions::glBufferData(gl_const::GLPixelBufferWrite, m_pixelBufferSize, nullptr, gl_const::GLDynamicDraw);
    GLFunctions::glBindBuffer(0, gl_const::GLPixelBufferWrite);
  }

  GLFunctions::glFlush();
  GLFunctions::glBindTexture(0);

#if defined(TRACK_GPU_MEM)
  uint32_t channelBitSize = 8;
  if (pixelType == gl_const::GL4BitOnChannel)
    channelBitSize = 4;

  uint32_t bitCount = channelBitSize * channelCount * m_width * m_height;
  uint32_t memSize = bitCount >> 3;
  dp::GPUMemTracker::Inst().AddAllocated("Texture", m_textureID, memSize);
  dp::GPUMemTracker::Inst().SetUsed("Texture", m_textureID, memSize);

  if (m_usePixelBuffer)
  {
    dp::GPUMemTracker::Inst().AddAllocated("PBO", m_pixelBufferID, m_pixelBufferSize);
    dp::GPUMemTracker::Inst().SetUsed("PBO", m_pixelBufferID, m_pixelBufferSize);
  }
#endif
}

void Texture::Destroy()
{
  if (m_textureID != -1)
  {
    GLFunctions::glDeleteTexture(m_textureID);
#if defined(TRACK_GPU_MEM)
    dp::GPUMemTracker::Inst().RemoveDeallocated("Texture", m_textureID);
    dp::GPUMemTracker::Inst().RemoveDeallocated("PBO", m_pixelBufferID);
#endif
    m_textureID = -1;
  }

  if (m_pixelBufferID != -1)
  {
    GLFunctions::glDeleteBuffer(m_pixelBufferID);
    m_pixelBufferID = -1;
  }
}

void Texture::SetFilterParams(glConst minFilter, glConst magFilter)
{
  ASSERT_ID;
  GLFunctions::glTexParameter(gl_const::GLMinFilter, minFilter);
  GLFunctions::glTexParameter(gl_const::GLMagFilter, magFilter);
}

void Texture::SetWrapMode(glConst sMode, glConst tMode)
{
  ASSERT_ID;
  GLFunctions::glTexParameter(gl_const::GLWrapS, sMode);
  GLFunctions::glTexParameter(gl_const::GLWrapT, tMode);
}

void Texture::UploadData(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                         TextureFormat format, ref_ptr<void> data)
{
  ASSERT_ID;
  ASSERT(format == m_format, ());
  glConst layout;
  glConst pixelType;
  uint32_t channelCount;

  UnpackFormat(format, layout, pixelType, channelCount);

  uint32_t const mappingSize = height * width * channelCount;
  if (m_usePixelBuffer && m_pixelBufferSize >= mappingSize)
  {
    GLFunctions::glBindBuffer(m_pixelBufferID, gl_const::GLPixelBufferWrite);
    GLFunctions::glBufferSubData(gl_const::GLPixelBufferWrite, mappingSize, data.get(), 0);
    GLFunctions::glTexSubImage2D(x, y, width, height, layout, pixelType, 0);
    GLFunctions::glBindBuffer(0, gl_const::GLPixelBufferWrite);
  }
  else
  {
    GLFunctions::glTexSubImage2D(x, y, width, height, layout, pixelType, data.get());
  }
}

TextureFormat Texture::GetFormat() const
{
  return m_format;
}

uint32_t Texture::GetWidth() const
{
  ASSERT_ID;
  return m_width;
}

uint32_t Texture::GetHeight() const
{
  ASSERT_ID;
  return m_height;
}

float Texture::GetS(uint32_t x) const
{
  ASSERT_ID;
  return x / (float)m_width;
}

float Texture::GetT(uint32_t y) const
{
  ASSERT_ID;
  return y / (float)m_height;
}

void Texture::Bind() const
{
  ASSERT_ID;
  GLFunctions::glBindTexture(GetID());
}

uint32_t Texture::GetMaxTextureSize()
{
  return GLFunctions::glGetInteger(gl_const::GLMaxTextureSize);
}

void Texture::UnpackFormat(TextureFormat format, glConst & layout, glConst & pixelType, uint32_t & channelsCount)
{
  switch (format)
  {
  case RGBA8:
    layout = gl_const::GLRGBA;
    pixelType = gl_const::GL8BitOnChannel;
    channelsCount = 4;
    break;
  case RGBA4:
    layout = gl_const::GLRGBA;
    pixelType = gl_const::GL4BitOnChannel;
    channelsCount = 4;
    break;
  case RED:
    layout = gl_const::GLRed;
    pixelType = gl_const::GL8BitOnChannel;
    channelsCount = 1;
    break;
  default:
    ASSERT(false, ());
    break;
  }
}

int32_t Texture::GetID() const
{
  return m_textureID;
}

} // namespace dp
