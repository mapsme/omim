#include "render/engine.hpp"
#include "render/feature_processor.hpp"
#include "render/window_handle.hpp"

#include "anim/controller.hpp"
#include "gui/controller.hpp"

#include "platform/video_timer.hpp"

#include "indexer/mercator.hpp"
#include "indexer/scales.hpp"
#include "base/strings_bundle.hpp"

namespace rg
{

Engine::Engine(Params && params)
  : m_navigator(m_scales)
  , m_drawFn(params.m_drawFn)
{
  m_animController.reset(new anim::Controller());
  RenderPolicy::Params rpParams = params.m_rpParams;
  rpParams.m_videoTimer->setFrameFn(bind(&Engine::DrawFrame, this));

  m_renderPolicy.reset(CreateRenderPolicy(rpParams));
  m_renderPolicy->SetRenderFn(bind(&Engine::DrawModel, this, _1, _2, _3, _4));
  m_scales.SetParams(m_renderPolicy->VisualScale(), m_renderPolicy->TileSize());
  m_renderPolicy->GetWindowHandle()->setUpdatesEnabled(true);

  Resize(params.m_rpParams.m_screenWidth, params.m_rpParams.m_screenHeight);
}

Engine::~Engine()
{
}

void Engine::InitGui(StringsBundle const & bundle)
{
  m_guiController.reset(new gui::Controller());
  m_guiController->SetStringsBundle(&bundle);

  gui::Controller::RenderParams rp(m_renderPolicy->Density(),
                                   bind(&WindowHandle::invalidate,
                                        m_renderPolicy->GetWindowHandle().get()),
                                   m_renderPolicy->GetGlyphCache(),
                                   m_renderPolicy->GetCacheScreen().get());

  m_guiController->SetRenderParams(rp);
  //m_informationDisplay.setVisualScale(m_renderPolicy->VisualScale());
  //m_balloonManager.RenderPolicyCreated(m_renderPolicy->Density());

  // init Bookmark manager
  //@{
//    graphics::Screen::Params pr;
//    pr.m_resourceManager = m_renderPolicy->GetResourceManager();
//    pr.m_threadSlot = m_renderPolicy->GetResourceManager()->guiThreadSlot();
//    pr.m_renderContext = m_renderPolicy->GetRenderContext();

//    pr.m_storageType = graphics::EMediumStorage;
//    pr.m_textureType = graphics::ESmallTexture;

  //m_bmManager.SetScreen(m_renderPolicy->GetCacheScreen().get());
  //@}

  // Do full invalidate instead of any "pending" stuff.
  Invalidate();
}

void Engine::Resize(int w, int h)
{
  w = max(w, 2);
  h = max(h, 2);

  m_navigator.OnSize(0, 0, w, h);
  // if gui controller not initialized, than we work in mode "Without gui"
  // and no need to set gui layout. We will not render it.
  //if (m_guiController)
  //  m_informationDisplay.SetWidgetPivotsByDefault(w, h);
  m_renderPolicy->OnSize(w, h);
}

void Engine::DrawFrame()
{
  shared_ptr<PaintEvent> pe(new PaintEvent(m_renderPolicy->GetDrawer().get()));

  if (m_renderPolicy->NeedRedraw())
  {
    m_renderPolicy->SetNeedRedraw(false);
    BeginPaint(pe);
    DoPaint(pe);
    EndPaint(pe);
  }
}

void Engine::ShowRect(m2::AnyRectD const & rect)
{
  m_navigator.SetFromRect(rect);
}

void Engine::BeginPaint(shared_ptr<PaintEvent> const  & e)
{
  m_renderPolicy->BeginFrame(e, m_navigator.Screen());
}

void Engine::DoPaint(shared_ptr<PaintEvent> const & e)
{
  m_renderPolicy->DrawFrame(e, m_navigator.Screen());

  // Don't render additional elements if guiController wasn't initialized.
  //if (m_guiController)
  //  DrawAdditionalInfo(e);
}

void Engine::EndPaint(shared_ptr<PaintEvent> const & e)
{
  m_renderPolicy->EndFrame(e, m_navigator.Screen());
}

void Engine::DrawModel(shared_ptr<PaintEvent> const & e, ScreenBase const & screen,
                       m2::RectD const & renderRect, int baseScale)
{
  m2::RectD selectRect;
  m2::RectD clipRect;

  double const inflationSize = m_scales.GetClipRectInflation();
  screen.PtoG(m2::Inflate(m2::RectD(renderRect), inflationSize, inflationSize), clipRect);
  screen.PtoG(m2::RectD(renderRect), selectRect);

  int drawScale = m_scales.GetDrawTileScale(baseScale);
  FeatureProcessor doDraw(clipRect, screen, e, drawScale);
  drawScale = min(drawScale, scales::GetUpperScale());

  try
  {
    m_drawFn(ref(doDraw), selectRect, drawScale, m_renderPolicy->IsTiling());
  }
  catch (redraw_operation_cancelled const &)
  {}

  e->setIsEmptyDrawing(doDraw.IsEmptyDrawing());
}

void Engine::Invalidate(bool forceUpdate)
{
  Invalidate(MercatorBounds::FullRect(), forceUpdate);
}

void Engine::Invalidate(m2::RectD const & rect, bool forceUpdate)
{
  ASSERT ( rect.IsValid(), () );
  m_renderPolicy->SetForceUpdate(forceUpdate);
  m_renderPolicy->SetInvalidRect(m2::AnyRectD(rect));
  m_renderPolicy->GetWindowHandle()->invalidate();
}

int Engine::GetWidth() const
{
  return m_navigator.Screen().PixelRect().SizeX();
}

int Engine::GetHeight() const
{
  return m_navigator.Screen().PixelRect().SizeY();
}

}
