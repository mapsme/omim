#pragma once

#include <boost/noncopyable.hpp>

namespace dp
{

class GLExtensionsList : private boost::noncopyable
{
public:
  enum ExtensionName
  {
    VertexArrayObject,
    TextureNPOT,
    RequiredInternalFormat,
    MapBuffer,
    UintIndices,
    MapBufferRange
  };

  static GLExtensionsList & Instance();
  bool IsSupported(ExtensionName const & extName) const;

private:
  GLExtensionsList();
  ~GLExtensionsList();

private:
  class Impl;
  Impl * m_impl;
};

} // namespace dp
