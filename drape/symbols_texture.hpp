#pragma once

#include "drape/texture.hpp"

#include <string>
#include <map>
#include <vector>

namespace dp
{

class SymbolsTexture : public Texture
{
public:
  class SymbolKey : public Key
  {
  public:
    SymbolKey(std::string const & symbolName);
    virtual ResourceType GetType() const;
    std::string const & GetSymbolName() const;

  private:
    std::string m_symbolName;
  };

  class SymbolInfo : public ResourceInfo
  {
  public:
    SymbolInfo(m2::RectF const & texRect);
    virtual ResourceType GetType() const;
  };

  SymbolsTexture(std::string const & skinPathName, std::string const & textureName,
                 ref_ptr<HWTextureAllocator> allocator);

  ref_ptr<ResourceInfo> FindResource(Key const & key, bool & newResource) override;

  void Invalidate(std::string const & skinPathName, ref_ptr<HWTextureAllocator> allocator);

  bool IsSymbolContained(std::string const & symbolName) const;

  static bool DecodeToMemory(std::string const & skinPathName, std::string const & textureName,
                             std::vector<uint8_t> & symbolsSkin,
                             std::map<std::string, m2::RectU> & symbolsIndex,
                             uint32_t & skinWidth, uint32_t & skinHeight);
private:
  void Fail();
  void Load(std::string const & skinPathName, ref_ptr<HWTextureAllocator> allocator);

  using TSymDefinition = std::map<std::string, SymbolInfo>;
  std::string m_name;
  mutable TSymDefinition m_definition;
};

} // namespace dp
