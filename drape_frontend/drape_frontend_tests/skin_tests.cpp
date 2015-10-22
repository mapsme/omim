#include "base/SRC_FIRST.hpp"
#include "testing/testing.hpp"

#include "drape_frontend/gui/skin.hpp"

UNIT_TEST(ParseDefaultSkinTest)
{
  gui::Skin skin(gui::ResolveGuiSkinFile("default"), 2.0);
  float width = 600.0f;
  float height = 800.0f;
  skin.Resize(width, height);

  gui::Position compassPos = skin.ResolvePosition(gui::WIDGET_COMPASS);
  TEST_EQUAL(compassPos.m_anchor, dp::Center, ());
  TEST_ALMOST_EQUAL(compassPos.m_pixelPivot.x, 27.0f * 2.0f, ());
  TEST_ALMOST_EQUAL(compassPos.m_pixelPivot.y, height - 57.0f * 2.0f, ());

  gui::Position rulerPos = skin.ResolvePosition(gui::WIDGET_RULER);
  TEST_EQUAL(rulerPos.m_anchor, dp::Right, ());
  TEST_ALMOST_EQUAL(rulerPos.m_pixelPivot.x, width - 6.0f * 2.0f, ());
  TEST_ALMOST_EQUAL(rulerPos.m_pixelPivot.y, height - 42.0f * 2.0f, ());

  gui::Position copyRightPos = skin.ResolvePosition(gui::WIDGET_COPYRIGHT);
  TEST_EQUAL(copyRightPos.m_anchor, dp::Right, ());
  TEST_ALMOST_EQUAL(copyRightPos.m_pixelPivot.x, width - 6.0f * 2.0f, ());
  TEST_ALMOST_EQUAL(copyRightPos.m_pixelPivot.y, height - 42.0f * 2.0f, ());

  {
    width = 800.0f;
    height = 600.0f;
    skin.Resize(width, height);

    gui::Position compassPos = skin.ResolvePosition(gui::WIDGET_COMPASS);
    TEST_EQUAL(compassPos.m_anchor, dp::Center, ());
    TEST_ALMOST_EQUAL(compassPos.m_pixelPivot.x, 18.0f * 2.0f, ());
    TEST_ALMOST_EQUAL(compassPos.m_pixelPivot.y, height - 11.4f * 2.0f, ());

    gui::Position rulerPos = skin.ResolvePosition(gui::WIDGET_RULER);
    TEST_EQUAL(rulerPos.m_anchor, dp::Right, ());
    TEST_ALMOST_EQUAL(rulerPos.m_pixelPivot.x, width - 70.4f * 2.0f, ());
    TEST_ALMOST_EQUAL(rulerPos.m_pixelPivot.y, height - 10.5f * 2.0f, ());

    gui::Position copyRightPos = skin.ResolvePosition(gui::WIDGET_COPYRIGHT);
    TEST_EQUAL(copyRightPos.m_anchor, dp::Right, ());
    TEST_ALMOST_EQUAL(copyRightPos.m_pixelPivot.x, width - 70.4f * 2.0f, ());
    TEST_ALMOST_EQUAL(copyRightPos.m_pixelPivot.y, height - 10.5f * 2.0f, ());
  }
}
