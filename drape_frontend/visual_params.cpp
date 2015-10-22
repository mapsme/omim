#include "drape_frontend/visual_params.hpp"

#include "base/macros.hpp"
#include "base/math.hpp"
#include "base/assert.hpp"

#include "indexer/mercator.hpp"

#include "std/limits.hpp"
#include "std/algorithm.hpp"

namespace df
{

uint32_t const YotaDevice = 0x1;

namespace
{

static VisualParams g_VizParams;

typedef pair<string, double> visual_scale_t;

} // namespace

#ifdef DEBUG
static bool g_isInited = false;
#define RISE_INITED g_isInited = true
#define ASSERT_INITED ASSERT(g_isInited, ())
#else
#define RISE_INITED
#define ASSERT_INITED
#endif


void VisualParams::Init(double vs, uint32_t tileSize, vector<uint32_t> const & additionalOptions)
{
  g_VizParams.m_tileSize = tileSize;
  g_VizParams.m_visualScale = vs;
  if (find(additionalOptions.begin(), additionalOptions.end(), YotaDevice) != additionalOptions.end())
    g_VizParams.m_isYotaDevice = true;

  // Here we set up glyphs rendering parameters separately for high-res and low-res screens.
  if (vs <= 1.0)
    g_VizParams.m_glyphVisualParams = { 0.43, 0.6, 0.615, 0.95, 0.43, 0.6 };
  else
    g_VizParams.m_glyphVisualParams = { 0.4, 0.575, 0.58, 0.95, 0.4, 0.575 };

  RISE_INITED;
}

uint32_t VisualParams::GetGlyphSdfScale() const
{
  return (m_visualScale <= 1.0) ? 3 : 4;
}

VisualParams & VisualParams::Instance()
{
  ASSERT_INITED;
  return g_VizParams;
}

string const & VisualParams::GetResourcePostfix(double visualScale, bool isYotaDevice)
{
  static visual_scale_t postfixes[] =
  {
    make_pair("ldpi", 0.75),
    make_pair("mdpi", 1.0),
    make_pair("hdpi", 1.5),
    make_pair("xhdpi", 2.0),
    make_pair("xxhdpi", 3.0),
    make_pair("6plus", 2.4),
  };

  static string specifixPostfixes[] =
  {
    "yota"
  };

  if (isYotaDevice)
    return specifixPostfixes[0];

  // Looking for the nearest available scale.
  int postfixIndex = -1;
  double minValue = numeric_limits<double>::max();
  for (int i = 0; i < ARRAY_SIZE(postfixes); i++)
  {
    double val = fabs(postfixes[i].second - visualScale);
    if (val < minValue)
    {
      minValue = val;
      postfixIndex = i;
    }
  }

  ASSERT_GREATER_OR_EQUAL(postfixIndex, 0, ());
  return postfixes[postfixIndex].first;
}

string const & VisualParams::GetResourcePostfix() const
{
  return VisualParams::GetResourcePostfix(m_visualScale, m_isYotaDevice);
}

double VisualParams::GetVisualScale() const
{
  return m_visualScale;
}

uint32_t VisualParams::GetTileSize() const
{
  return m_tileSize;
}

uint32_t VisualParams::GetTouchRectRadius() const
{
  return 20 * GetVisualScale();
}

double VisualParams::GetDragThreshold() const
{
  return 10.0 * GetVisualScale();
}

VisualParams::GlyphVisualParams const & VisualParams::GetGlyphVisualParams() const
{
  return m_glyphVisualParams;
}

VisualParams::VisualParams()
  : m_tileSize(0)
  , m_visualScale(0.0)
{
}

m2::RectD const & GetWorldRect()
{
  static m2::RectD const worldRect = MercatorBounds::FullRect();
  return worldRect;
}

int GetTileScaleBase(ScreenBase const & s, uint32_t tileSize)
{
  ScreenBase tmpS = s;
  tmpS.Rotate(-tmpS.GetAngle());

  // slightly smaller than original to produce "antialiasing" effect using bilinear filtration.
  int const halfSize = static_cast<int>(tileSize / 1.05 / 2.0);

  m2::RectD glbRect;
  m2::PointD const pxCenter = tmpS.PixelRect().Center();
  tmpS.PtoG(m2::RectD(pxCenter - m2::PointD(halfSize, halfSize),
                      pxCenter + m2::PointD(halfSize, halfSize)),
            glbRect);

  return GetTileScaleBase(glbRect);
}

int GetTileScaleBase(ScreenBase const & s)
{
  return GetTileScaleBase(s, VisualParams::Instance().GetTileSize());
}

int GetTileScaleBase(m2::RectD const & r)
{
  double const sz = max(r.SizeX(), r.SizeY());
  return max(1, my::rounds(log((MercatorBounds::maxX - MercatorBounds::minX) / sz) / log(2.0)));
}

int GetTileScaleIncrement(uint32_t tileSize, double visualScale)
{
  return log(tileSize / 256.0 / visualScale) / log(2.0);
}

int GetTileScaleIncrement()
{
  VisualParams const & p = VisualParams::Instance();
  return GetTileScaleIncrement(p.GetTileSize(), p.GetVisualScale());
}

m2::RectD GetRectForDrawScale(int drawScale, m2::PointD const & center, uint32_t tileSize, double visualScale)
{
  // +1 - we will calculate half length for each side
  double const factor = 1 << (max(1, drawScale - GetTileScaleIncrement(tileSize, visualScale)) + 1);

  double const len = (MercatorBounds::maxX - MercatorBounds::minX) / factor;

  return m2::RectD(MercatorBounds::ClampX(center.x - len),
                   MercatorBounds::ClampY(center.y - len),
                   MercatorBounds::ClampX(center.x + len),
                   MercatorBounds::ClampY(center.y + len));
}

m2::RectD GetRectForDrawScale(int drawScale, m2::PointD const & center)
{
  VisualParams const & p = VisualParams::Instance();
  return GetRectForDrawScale(drawScale, center, p.GetTileSize(), p.GetVisualScale());
}

m2::RectD GetRectForDrawScale(double drawScale, m2::PointD const & center, uint32_t tileSize, double visualScale)
{
  return GetRectForDrawScale(my::rounds(drawScale), center, tileSize, visualScale);
}

m2::RectD GetRectForDrawScale(double drawScale, m2::PointD const & center)
{
  return GetRectForDrawScale(my::rounds(drawScale), center);
}

int CalculateTileSize(int screenWidth, int screenHeight)
{
  int const maxSz = max(screenWidth, screenHeight);

  // we're calculating the tileSize based on (maxSz > 1024 ? rounded : ceiled)
  // to the nearest power of two value of the maxSz

  int const ceiledSz = 1 << static_cast<int>(ceil(log(double(maxSz + 1)) / log(2.0)));
  int res = 0;

  if (maxSz < 1024)
    res = ceiledSz;
  else
  {
    int const flooredSz = ceiledSz / 2;
    // rounding to the nearest power of two.
    if (ceiledSz - maxSz < maxSz - flooredSz)
      res = ceiledSz;
    else
      res = flooredSz;
  }

#ifndef OMIM_OS_DESKTOP
  return my::clamp(res / 2, 256, 1024);
#else
  return my::clamp(res / 2, 512, 1024);
#endif
}

int GetDrawTileScale(int baseScale, uint32_t tileSize, double visualScale)
{
  return max(1, baseScale + GetTileScaleIncrement(tileSize, visualScale));
}

int GetDrawTileScale(ScreenBase const & s, uint32_t tileSize, double visualScale)
{
  return GetDrawTileScale(GetTileScaleBase(s, tileSize), tileSize, visualScale);
}

int GetDrawTileScale(m2::RectD const & r, uint32_t tileSize, double visualScale)
{
  return GetDrawTileScale(GetTileScaleBase(r), tileSize, visualScale);
}

int GetDrawTileScale(int baseScale)
{
  VisualParams const & p = VisualParams::Instance();
  return GetDrawTileScale(baseScale, p.GetTileSize(), p.GetVisualScale());
}

int GetDrawTileScale(ScreenBase const & s)
{
  VisualParams const & p = VisualParams::Instance();
  return GetDrawTileScale(s, p.GetTileSize(), p.GetVisualScale());
}

int GetDrawTileScale(m2::RectD const & r)
{
  VisualParams const & p = VisualParams::Instance();
  return GetDrawTileScale(r, p.GetTileSize(), p.GetVisualScale());
}

} // namespace df
