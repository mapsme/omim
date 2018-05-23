#include "drape/static_texture.hpp"

#include "indexer/map_style_reader.hpp"

#include "platform/platform.hpp"

#include "coding/parse_xml.hpp"
#include "coding/reader.hpp"

#include "base/string_utils.hpp"

#ifdef DEBUG
#include "3party/glm/glm/gtx/bit.hpp"
#endif
#include "3party/stb_image/stb_image.h"

#include <vector>

namespace dp
{
std::string const StaticTexture::kDefaultResource = "default";

namespace
{
using TLoadingCompletion = function<void(unsigned char *, uint32_t, uint32_t)>;
using TLoadingFailure = function<void(std::string const &)>;

bool LoadData(std::string const & textureName, std::string const & skinPathName,
              uint8_t bytesPerPixel, TLoadingCompletion const & completionHandler,
              TLoadingFailure const & failureHandler)
{
  ASSERT(completionHandler != nullptr, ());
  ASSERT(failureHandler != nullptr, ());

  std::vector<unsigned char> rawData;
  try
  {
    std::string const fullName = textureName + ".png";
    ReaderPtr<Reader> reader = skinPathName == StaticTexture::kDefaultResource ?
                               GetStyleReader().GetDefaultResourceReader(fullName) :
                               GetStyleReader().GetResourceReader(fullName, skinPathName);

    CHECK_LESS(reader.Size(), static_cast<uint64_t>(numeric_limits<size_t>::max()), ());
    size_t const size = static_cast<size_t>(reader.Size());
    rawData.resize(size);
    reader.Read(0, &rawData[0], size);
  }
  catch (RootException & e)
  {
    failureHandler(e.what());
    return false;
  }

  int w, h, bpp;
  unsigned char * data =
      stbi_load_from_memory(&rawData[0], static_cast<int>(rawData.size()), &w, &h, &bpp, 0);

  uint8_t constexpr kSupportedBPP = 4;
  CHECK_EQUAL(bpp, kSupportedBPP, ("Incorrect texture format"));
  ASSERT(glm::isPowerOfTwo(w), (w));
  ASSERT(glm::isPowerOfTwo(h), (h));

  if (bytesPerPixel != bpp)
  {
    std::vector<unsigned char> convertedData(static_cast<size_t>(w * h * bytesPerPixel));
    uint32_t const pixelsCount = static_cast<uint32_t>(w * h);
    for (uint32_t i = 0; i < pixelsCount; i++)
    {
      unsigned char const * p = data + i * bpp;
      for (uint8_t b = 0; b < bytesPerPixel; b++)
        convertedData[i * bytesPerPixel + b] = p[b];
    }
    stbi_image_free(data);
    completionHandler(convertedData.data(), static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    return true;
  }

  completionHandler(data, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
  stbi_image_free(data);
  return true;
}

class StaticResourceInfo : public Texture::ResourceInfo
{
public:
  StaticResourceInfo() : Texture::ResourceInfo(m2::RectF(0.0f, 0.0f, 1.0f, 1.0f)) {}
  virtual ~StaticResourceInfo() {}
  Texture::ResourceType GetType() const override { return Texture::Static; }
};
}  // namespace

StaticTexture::StaticTexture(std::string const & textureName, std::string const & skinPathName,
                             dp::TextureFormat format, ref_ptr<HWTextureAllocator> allocator)
  : m_textureName(textureName)
  , m_skinPathName(skinPathName)
  , m_format(format)
  , m_info(make_unique_dp<StaticResourceInfo>())
{
  m_isLoadingCorrect = Load(allocator);
}

bool StaticTexture::Load(ref_ptr<HWTextureAllocator> allocator)
{
  auto completionHandler = [this, &allocator](unsigned char * data, uint32_t width,
                                              uint32_t height)
  {
    Texture::Params p;
    p.m_allocator = allocator;
    p.m_format = m_format;
    p.m_width = width;
    p.m_height = height;
    p.m_wrapSMode = gl_const::GLRepeate;
    p.m_wrapTMode = gl_const::GLRepeate;

    Create(p, make_ref(data));
  };

  auto failureHandler = [this](std::string const & reason)
  {
    LOG(LERROR, (reason));
    Fail();
  };

  uint8_t const bytesPerPixel = GetBytesPerPixel(m_format);
  return LoadData(m_textureName, m_skinPathName, bytesPerPixel,
                  completionHandler, failureHandler);
}

void StaticTexture::Invalidate(ref_ptr<HWTextureAllocator> allocator)
{
  Destroy();
  m_isLoadingCorrect = Load(allocator);
}

ref_ptr<Texture::ResourceInfo> StaticTexture::FindResource(Texture::Key const & key,
                                                           bool & newResource)
{
  newResource = false;
  if (key.GetType() != Texture::Static)
    return nullptr;
  return make_ref(m_info);
}

void StaticTexture::Fail()
{
  int32_t alphaTexture = 0;
  Texture::Params p;
  p.m_allocator = GetDefaultAllocator();
  p.m_format = dp::RGBA8;
  p.m_width = 1;
  p.m_height = 1;
  Create(p, make_ref(&alphaTexture));
}
}  // namespace dp
