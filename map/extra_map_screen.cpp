#include "extra_map_screen.h"
#include "framework.hpp"

// @todo(VB) this global value is a temprary hack to check something.
// It should be redisigned.
ExtraMapScreen::TImageReadyCallback g_imageReady = nullptr;

ExtraMapScreen::ExtraMapScreen(Framework * framework)
  : m_framework(framework)
{
  ASSERT(framework, ());
}

void ExtraMapScreen::SetRenderAsyncCallback(TImageReadyCallback imageReady)
{
  g_imageReady = imageReady;
}

void ExtraMapScreen::RenderAsync(m2::PointD const & center, size_t scale, size_t width, size_t height)
{
  m_framework->SetViewportCenter(center);
}
