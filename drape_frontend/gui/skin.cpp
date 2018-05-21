#include "drape_frontend/gui/skin.hpp"

#include "platform/platform.hpp"

#include "coding/parse_xml.hpp"

#include "base/string_utils.hpp"

#include <memory>

namespace gui
{
namespace
{
#ifdef DEBUG
bool IsSimple(dp::Anchor anchor)
{
  return anchor >= 0 && anchor <= 8;
}

bool IsAnchor(dp::Anchor anchor)
{
  return anchor >= 0 && anchor <= 10;
}
#endif

dp::Anchor ParseValueAnchor(std::string const & value)
{
  if (value == "center")
    return dp::Center;
  if (value == "left")
    return dp::Left;
  if (value == "right")
    return dp::Right;
  if (value == "top")
    return dp::Top;
  if (value == "bottom")
    return dp::Bottom;
  else
    ASSERT(false, ());

  return dp::Center;
}

dp::Anchor MergeAnchors(dp::Anchor src, dp::Anchor dst)
{
  ASSERT(IsSimple(dst), ());
  ASSERT(IsSimple(src), ());
  dp::Anchor result = static_cast<dp::Anchor>(src | dst);
  ASSERT(IsAnchor(result), ());
  return result;
}

float ParseFloat(std::string const & v)
{
  double d = 0.0;
  VERIFY(strings::to_double(v, d), ());
  return static_cast<float>(d);
}

class ResolverParser
{
public:
  enum class Element
  {
    Empty,
    Anchor,
    Relative,
    Offset
  };

  ResolverParser()
    : m_element(Element::Empty)
  {}

  void Parse(std::string const & attr, std::string const & value)
  {
    ASSERT(m_element != Element::Empty, ());

    if (attr == "x")
    {
      ASSERT(m_element == Element::Offset, ());
      m_resolver.SetOffsetX(ParseFloat(value));
    }
    else if (attr == "y")
    {
      ASSERT(m_element == Element::Offset, ());
      m_resolver.SetOffsetY(ParseFloat(value));
    }
    else if (attr == "vertical" || attr == "horizontal")
    {
      if (m_element == Element::Anchor)
        m_resolver.AddAnchor(ParseValueAnchor(value));
      else if (m_element == Element::Relative)
        m_resolver.AddRelative(ParseValueAnchor(value));
      else
        ASSERT(false, ());
    }
  }

  void Reset()
  {
    m_resolver = PositionResolver();
  }

  PositionResolver const & GetResolver() const
  {
    return m_resolver;
  }

  void SetElement(Element e)
  {
    m_element = e;
  }

private:
  Element m_element;
  PositionResolver m_resolver;
};

class SkinLoader
{
public:
  explicit SkinLoader(std::map<EWidget, std::pair<PositionResolver, PositionResolver>> & skin)
    : m_skin(skin)
  {}

  bool Push(std::string const & element)
  {
    if (!m_inElement)
    {
      if (element == "root")
        return true;

      m_inElement = true;
      if (element == "ruler")
        m_currentElement = WIDGET_RULER;
      else if (element == "compass")
        m_currentElement = WIDGET_COMPASS;
      else if (element == "copyright")
        m_currentElement = WIDGET_COPYRIGHT;
      else if (element == "watermark")
        m_currentElement = WIDGET_WATERMARK;
      else
        ASSERT(false, ());
    }
    else if (!m_inConfiguration)
    {
      if (element == "portrait" || element == "landscape")
        m_inConfiguration = true;
      else
        ASSERT(false, ());
    }
    else
    {
      if (element == "anchor")
        m_parser.SetElement(ResolverParser::Element::Anchor);
      else if (element == "relative")
        m_parser.SetElement(ResolverParser::Element::Relative);
      else if (element == "offset")
        m_parser.SetElement(ResolverParser::Element::Offset);
      else
        ASSERT(false, ());
    }

    return true;
  }

  void Pop(std::string const & element)
  {
    if (element == "anchor" || element == "relative" || element == "offset")
      m_parser.SetElement(ResolverParser::Element::Empty);
    else if (element == "portrait")
    {
      m_skin[m_currentElement].first = m_parser.GetResolver();
      m_parser.Reset();
      m_inConfiguration = false;
    }
    else if (element == "landscape")
    {
      m_skin[m_currentElement].second = m_parser.GetResolver();
      m_parser.Reset();
      m_inConfiguration = false;
    }
    else if (element == "ruler" || element == "compass" || element == "copyright" ||
             element == "country_status" || element == "watermark")
    {
      m_inElement = false;
    }
  }

  void AddAttr(std::string const & attribute, std::string const & value)
  {
    m_parser.Parse(attribute, value);
  }

  void CharData(std::string const &) {}

private:
  bool m_inConfiguration = false;
  bool m_inElement = false;

  EWidget m_currentElement = WIDGET_RULER;
  ResolverParser m_parser;

  map<EWidget, pair<PositionResolver, PositionResolver> > & m_skin;
};
}

Position PositionResolver::Resolve(int w, int h, double vs) const
{
  float resultX = w / 2.0f;
  float resultY = h / 2.0f;
  m2::PointF offset = m_offset * vs;

  if (m_resolveAnchor & dp::Left)
   resultX = offset.x;
  else if (m_resolveAnchor & dp::Right)
   resultX = w - offset.x;
  else
   resultX += offset.x;

  if (m_resolveAnchor & dp::Top)
   resultY = offset.y;
  else if (m_resolveAnchor & dp::Bottom)
   resultY = h - offset.y;
  else
   resultY += offset.y;

  return Position(m2::PointF(resultX, resultY), m_elementAnchor);
}

void PositionResolver::AddAnchor(dp::Anchor anchor)
{
  m_elementAnchor = MergeAnchors(m_elementAnchor, anchor);
}

void PositionResolver::AddRelative(dp::Anchor anchor)
{
  m_resolveAnchor = MergeAnchors(m_resolveAnchor, anchor);
}

void PositionResolver::SetOffsetX(float x)
{
  m_offset.x = x;
}

void PositionResolver::SetOffsetY(float y)
{
  m_offset.y = y;
}

Skin::Skin(ReaderPtr<Reader> const & reader, float visualScale)
  : m_visualScale(visualScale)
{
  SkinLoader loader(m_resolvers);
  ReaderSource<ReaderPtr<Reader> > source(reader);
  if (!ParseXML(source, loader))
    LOG(LERROR, ("Error parsing gui skin"));
}

Position Skin::ResolvePosition(EWidget name)
{
  // check that name have only one bit
  ASSERT((static_cast<int>(name) & (static_cast<int>(name) - 1)) == 0, ());
  TResolversPair const & resolvers = m_resolvers[name];
  PositionResolver const & resolver = (m_displayWidth < m_displayHeight) ? resolvers.first : resolvers.second;
  return resolver.Resolve(m_displayWidth, m_displayHeight, m_visualScale);
}

void Skin::Resize(int w, int h)
{
  m_displayWidth = w;
  m_displayHeight = h;
}

ReaderPtr<Reader> ResolveGuiSkinFile(std::string const & deviceType)
{
  Platform & pl = GetPlatform();
  std::unique_ptr<Reader> reader;
  try
  {
    reader = pl.GetReader("resources-default/" + deviceType + ".ui");
  }
  catch(FileAbsentException & e)
  {
    LOG(LINFO, ("Gui skin for : ", deviceType ,"not found"));
  }

  if (!reader)
  {
    try
    {
      reader = pl.GetReader("resources-default/default.ui");
    }
    catch(FileAbsentException & e)
    {
      LOG(LINFO, ("Default gui skin not found"));
      throw e;
    }
  }

  return ReaderPtr<Reader>(std::move(reader));
}
}  // namespace gui
