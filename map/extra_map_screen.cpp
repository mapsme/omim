#include "extra_map_screen.h"
#include "framework.hpp"

// @todo(VB) this global value is a temprary hack to check something.
// It should be redisigned.
ExtraMapScreen::TImageReadyCallback g_imageReady = nullptr;

ExtraMapScreen::ExtraMapScreen(Framework * framework, TImageReadyCallback imageReady)
  : m_framework(framework)
{
  g_imageReady = imageReady;
  ASSERT(framework, ());
}

void ExtraMapScreen::RenderAsync(m2::PointD const & center, size_t scale, size_t width, size_t height)
{
  m_framework->SetViewportCenter(center);
}
