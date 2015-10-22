#include "drape/symbols_texture.hpp"
#include "3party/stb_image/stb_image.h"

#include "indexer/map_style_reader.hpp"

#include "platform/platform.hpp"

#include "coding/reader.hpp"
#include "coding/parse_xml.hpp"

#include "base/string_utils.hpp"

namespace dp
{

namespace
{

string const SymbolsTextureName = "symbols";

using TDefinitionInserter = function<void(string const &, m2::RectF const &)>;
using TSymbolsLoadingCompletion = function<void(unsigned char *, uint32_t, uint32_t)>;
using TSymbolsLoadingFailure = function<void(string const &)>;

class DefinitionLoader
{
public:
  DefinitionLoader(TDefinitionInserter const & definitionInserter, bool convertToUV)
    : m_definitionInserter(definitionInserter)
    , m_convertToUV(convertToUV)
    , m_width(0)
    , m_height(0)
  {
  }

  bool Push(string const & /*element*/) { return true;}

  void Pop(string const & element)
  {
    if (element == "symbol")
    {
      ASSERT(!m_name.empty(), ());
      ASSERT(m_rect.IsValid(), ());
      ASSERT(m_definitionInserter != nullptr, ());
      m_definitionInserter(m_name, m_rect);

      m_name = "";
      m_rect.MakeEmpty();
    }
  }

  void AddAttr(string const & attribute, string const & value)
  {
    if (attribute == "name")
      m_name = value;
    else
    {
      int v;
      if (!strings::to_int(value, v))
        return;

      if (attribute == "minX")
      {
        ASSERT(m_width != 0, ());
        float const scalar = m_convertToUV ? static_cast<float>(m_width) : 1.0f;
        m_rect.setMinX(v / scalar);
      }
      else if (attribute == "minY")
      {
        ASSERT(m_height != 0, ());
        float const scalar = m_convertToUV ? static_cast<float>(m_height) : 1.0f;
        m_rect.setMinY(v / scalar);
      }
      else if (attribute == "maxX")
      {
        ASSERT(m_width != 0, ());
        float const scalar = m_convertToUV ? static_cast<float>(m_width) : 1.0f;
        m_rect.setMaxX(v / scalar);
      }
      else if (attribute == "maxY")
      {
        ASSERT(m_height != 0, ());
        float const scalar = m_convertToUV ? static_cast<float>(m_height) : 1.0f;
        m_rect.setMaxY(v / scalar);
      }
      else if (attribute == "height")
      {
        m_height = v;
      }
      else if (attribute == "width")
      {
        m_width = v;
      }
    }
  }

  void CharData(string const &) {}

  uint32_t GetWidth() const { return m_width; }
  uint32_t GetHeight() const { return m_height; }

private:
  TDefinitionInserter m_definitionInserter;
  bool m_convertToUV;

  uint32_t m_width;
  uint32_t m_height;

  string m_name;
  m2::RectF m_rect;
};

void LoadSymbols(string const & skinPathName, bool convertToUV,
                 TDefinitionInserter const & definitionInserter,
                 TSymbolsLoadingCompletion const & completionHandler,
                 TSymbolsLoadingFailure const & failureHandler)
{
  ASSERT(definitionInserter != nullptr, ());
  ASSERT(completionHandler != nullptr, ());
  ASSERT(failureHandler != nullptr, ());

  vector<unsigned char> rawData;
  uint32_t width, height;

  try
  {
    DefinitionLoader loader(definitionInserter, convertToUV);

    {
      ReaderPtr<Reader> reader = GetStyleReader().GetResourceReader(SymbolsTextureName + ".sdf", skinPathName);
      ReaderSource<ReaderPtr<Reader> > source(reader);
      if (!ParseXML(source, loader))
      {
        failureHandler("Error parsing skin");
        return;
      }

      width = loader.GetWidth();
      height = loader.GetHeight();
    }

    {
      ReaderPtr<Reader> reader = GetStyleReader().GetResourceReader(SymbolsTextureName + ".png", skinPathName);
      size_t const size = reader.Size();
      rawData.resize(size);
      reader.Read(0, &rawData[0], size);
    }
  }
  catch (RootException & e)
  {
    failureHandler(e.what());
    return;
  }

  int w, h, bpp;
  unsigned char * data = stbi_png_load_from_memory(&rawData[0], rawData.size(), &w, &h, &bpp, 0);
  ASSERT_EQUAL(bpp, 4, ("Incorrect symbols texture format"));

  if (width == w && height == h)
  {
    completionHandler(data, width, height);
  }
  else
  {
    failureHandler("Error symbols texture creation");
  }

  stbi_image_free(data);
}

}

SymbolsTexture::SymbolKey::SymbolKey(string const & symbolName)
  : m_symbolName(symbolName)
{
}

Texture::ResourceType SymbolsTexture::SymbolKey::GetType() const
{
  return Texture::Symbol;
}

string const & SymbolsTexture::SymbolKey::GetSymbolName() const
{
  return m_symbolName;
}

SymbolsTexture::SymbolInfo::SymbolInfo(const m2::RectF & texRect)
  : ResourceInfo(texRect)
{
}

Texture::ResourceType SymbolsTexture::SymbolInfo::GetType() const
{
  return Symbol;
}

SymbolsTexture::SymbolsTexture(string const & skinPathName, ref_ptr<HWTextureAllocator> allocator)
{
  Load(skinPathName, allocator);
}

void SymbolsTexture::Load(string const & skinPathName, ref_ptr<HWTextureAllocator> allocator)
{
  auto definitionInserter = [this](string const & name, m2::RectF const & rect)
  {
    m_definition.insert(make_pair(name, SymbolsTexture::SymbolInfo(rect)));
  };

  auto completionHandler = [this, &allocator](unsigned char * data, uint32_t width, uint32_t height)
  {
    Texture::Params p;
    p.m_allocator = allocator;
    p.m_format = dp::RGBA8;
    p.m_width = width;
    p.m_height = height;

    Create(p, make_ref(data));
  };

  auto failureHandler = [this](string const & reason)
  {
    LOG(LERROR, (reason));
    Fail();
  };

  LoadSymbols(skinPathName, true /* convertToUV */, definitionInserter, completionHandler, failureHandler);
}

void SymbolsTexture::Invalidate(string const & skinPathName, ref_ptr<HWTextureAllocator> allocator)
{
  Destroy();
  m_definition.clear();

  Load(skinPathName, allocator);
}

ref_ptr<Texture::ResourceInfo> SymbolsTexture::FindResource(Texture::Key const & key, bool & newResource)
{
  newResource = false;
  if (key.GetType() != Texture::Symbol)
    return nullptr;

  string const & symbolName = static_cast<SymbolKey const &>(key).GetSymbolName();

  TSymDefinition::iterator it = m_definition.find(symbolName);
  ASSERT(it != m_definition.end(), (symbolName));
  return make_ref(&it->second);
}

void SymbolsTexture::Fail()
{
  m_definition.clear();
  int32_t alfaTexture = 0;
  Texture::Params p;
  p.m_allocator = GetDefaultAllocator();
  p.m_format = dp::RGBA8;
  p.m_width = 1;
  p.m_height = 1;

  Create(p, make_ref(&alfaTexture));
}

bool SymbolsTexture::DecodeToMemory(string const & skinPathName, vector<uint8_t> & symbolsSkin,
                                    map<string, m2::RectU> & symbolsIndex,
                                    uint32_t & skinWidth, uint32_t & skinHeight)
{
  auto definitionInserter = [&symbolsIndex](string const & name, m2::RectF const & rect)
  {
    symbolsIndex.insert(make_pair(name, m2::RectU(rect)));
  };

  bool result = true;
  auto completionHandler = [&result, &symbolsSkin, &skinWidth, &skinHeight](unsigned char * data,
      uint32_t width, uint32_t height)
  {
    size_t size = 4 * width * height;
    symbolsSkin.resize(size);
    memcpy(symbolsSkin.data(), data, size);
    skinWidth = width;
    skinHeight = height;
    result = true;
  };

  auto failureHandler = [&result](string const & reason)
  {
    LOG(LERROR, (reason));
    result = false;
  };

  LoadSymbols(skinPathName, false /* convertToUV */, definitionInserter, completionHandler, failureHandler);
  return result;
}

} // namespace dp
