/// @author Siarhei Rachytski
#pragma once

#include "geometry/rect2d.hpp"

#include "std/function.hpp"

namespace graphics
{

class SkinLoader
{
private:
  uint32_t m_texMinX;
  uint32_t m_texMinY;
  uint32_t m_texMaxX;
  uint32_t m_texMaxY;
  string m_resID;
  int m_key;

public:

  /// @param _1 - symbol rect on texture. Pixel rect
  /// @param _2 - symbol name. Key on resource search
  using TIconCallback = function<void (m2::RectU const &, string const &, int)>;

  SkinLoader(TIconCallback const & callback);

  bool Push(string const & element);
  void Pop(string const & element);
  void AddAttr(string const & attribute, string const & value);
  void CharData(string const &) {}

private:
  TIconCallback m_callback;

  using TAttributeProcess = function<void (string const & value)>;
  map<string, TAttributeProcess> m_processors;
};

} // namespace graphics
