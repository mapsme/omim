#pragma once

#include "drape/texture.hpp"

#include <string>

namespace dp
{

class StaticTexture : public Texture
{
public:
  class StaticKey : public Key
  {
  public:
    ResourceType GetType() const override { return ResourceType::Static; }
  };

  StaticTexture(std::string const & textureName, std::string const & skinPathName,
                ref_ptr<HWTextureAllocator> allocator);

  ref_ptr<ResourceInfo> FindResource(Key const & key, bool & newResource) override;

  void Invalidate(std::string const & skinPathName, ref_ptr<HWTextureAllocator> allocator);

  bool IsLoadingCorrect() const { return m_isLoadingCorrect; }

private:
  void Fail();
  bool Load(std::string const & skinPathName, ref_ptr<HWTextureAllocator> allocator);

  std::string m_textureName;
  drape_ptr<Texture::ResourceInfo> m_info;

  bool m_isLoadingCorrect;
};

} // namespace dp
