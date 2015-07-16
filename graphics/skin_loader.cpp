#include "graphics/skin_loader.hpp"

#include "base/string_utils.hpp"

#include "std/bind.hpp"

namespace graphics
{

int StrToInt(string const & s)
{
  int i = 0;
  VERIFY(strings::to_int(s, i), ("Bad int int StrToInt function"));
  return i;
}

SkinLoader::SkinLoader(TIconCallback const & callback)
: m_callback(callback)
, m_key(0)
{
  m_processors["minX"] = [this](string const & value) { m_texMinX = StrToInt(value);};
  m_processors["maxX"] = [this](string const & value) { m_texMaxX = StrToInt(value);};
  m_processors["minY"] = [this](string const & value) { m_texMinY = StrToInt(value);};
  m_processors["maxY"] = [this](string const & value) { m_texMaxY = StrToInt(value);};
  m_processors["name"] = [this](string const & value) { m_resID = value; };
}

bool SkinLoader::Push(string const & /*element*/)
{
  return true;
}

void SkinLoader::Pop(string const & element)
{
  if (element == "symbol")
    m_callback(m2::RectU(m_texMinX, m_texMinY, m_texMaxX, m_texMaxY), m_resID, m_key++);
}

void SkinLoader::AddAttr(string const & attr, string const & value)
{
  auto const iter = m_processors.find(attr);
  if (iter == m_processors.end())
    return;

  iter->second(value);
}

} // namespace graphics
