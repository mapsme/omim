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

Texture::Texture()
{}

Texture::~Texture()
{
  Destroy();
}

void Texture::Create(Params const & params, ref_ptr<void> data)
{
  m_сreationParams = params;
  if (!GLExtensionsList::Instance().IsSupported(GLExtensionsList::TextureNPOT))
  {
    m_сreationParams.m_width = my::NextPowOf2(m_сreationParams.m_width);
    m_сreationParams.m_height = my::NextPowOf2(m_сreationParams.m_height);
  }

  if (data != nullptr && AllocateTexture(m_сreationParams.m_allocator))
    m_hwTexture->Create(m_сreationParams, data);
}

void Texture::UploadData(uint32_t x, uint32_t y, uint32_t width, uint32_t height, ref_ptr<void> data)
{
  ASSERT(m_hwTexture != nullptr, ());
  m_hwTexture->UploadData(x, y, width, height, data);
}

TextureFormat Texture::GetFormat() const
{
  return m_сreationParams.m_format;
}

uint32_t Texture::GetWidth() const
{
  return m_сreationParams.m_width;
}

uint32_t Texture::GetHeight() const
{
  return m_сreationParams.m_height;
}

float Texture::GetS(uint32_t x) const
{
  return x / (float)m_сreationParams.m_width;
}

float Texture::GetT(uint32_t y) const
{
  return y / (float)m_сreationParams.m_height;
}

void Texture::Bind()
{
  if (m_hwTexture == nullptr && AllocateTexture(m_сreationParams.m_allocator))
    m_hwTexture->Create(m_сreationParams, nullptr);

  ASSERT(m_hwTexture != nullptr, ());
  m_hwTexture->Bind();
}

void Texture::SetFilter(glConst filter)
{
  ASSERT(m_hwTexture != nullptr, ());
  m_hwTexture->SetFilter(filter);
}

void Texture::Destroy()
{
  m_hwTexture.reset();
}

bool Texture::AllocateTexture(ref_ptr<HWTextureAllocator> allocator)
{
  if (allocator != nullptr)
  {
    m_hwTexture = allocator->CreateTexture();
    return true;
  }

  return false;
}

} // namespace dp
