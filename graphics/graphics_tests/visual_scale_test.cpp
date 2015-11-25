#include "testing/testing.hpp"

#include "graphics/defines.hpp"


using namespace graphics;

UNIT_TEST(SupportedVisualScaleTest)
{
  TEST_ALMOST_EQUAL_ULPS(supportedVisualScale(0.), 1., ());
  TEST_ALMOST_EQUAL_ULPS(supportedVisualScale(0.75), 1., ());
  TEST_ALMOST_EQUAL_ULPS(supportedVisualScale(0.99), 1., ());
  TEST_ALMOST_EQUAL_ULPS(supportedVisualScale(1.24), 1., ());
  TEST_ALMOST_EQUAL_ULPS(supportedVisualScale(1.26), 1.5, ());
  TEST_ALMOST_EQUAL_ULPS(supportedVisualScale(3.6), 4., ());
  TEST_ALMOST_EQUAL_ULPS(supportedVisualScale(4.6), 4., ());
  TEST_ALMOST_EQUAL_ULPS(supportedVisualScale(10.), 4., ());
}

UNIT_TEST(VisualScaleTest)
{
  TEST_ALMOST_EQUAL_ULPS(visualScaleExact(130), 1., ());
  TEST_ALMOST_EQUAL_ULPS(visualScaleExact(159), 1., ());
  TEST_ALMOST_EQUAL_ULPS(visualScaleExact(161), 1., ());
  TEST_ALMOST_EQUAL_ULPS(visualScaleExact(199), 1., ());
  TEST_ALMOST_EQUAL_ULPS(visualScaleExact(201), 1.5, ());
  TEST_ALMOST_EQUAL_ULPS(visualScaleExact(500), 3., ());
  TEST_ALMOST_EQUAL_ULPS(visualScaleExact(538), 3., ());
  TEST_ALMOST_EQUAL_ULPS(visualScaleExact(600), 4., ());
  TEST_ALMOST_EQUAL_ULPS(visualScaleExact(800), 4., ());
}
