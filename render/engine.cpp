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

namespace
{

unsigned const LONG_TOUCH_MS = 1000;
unsigned const SHORT_TOUCH_MS = 250;
double const DOUBLE_TOUCH_S = SHORT_TOUCH_MS / 1000.0;

} // namespace

Engine::Engine(Params && params)
  : m_navigator(m_scales)
  , m_drawFn(params.m_drawFn)
{
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

void Engine::Scale(double factor, const m2::PointD & pxPoint, bool isAnim)
{
  if (isAnim)
    m_renderPolicy->AddAnimTask(m_navigator.ScaleToPointAnim(pxPoint, factor, 0.25));
  else
    m_navigator.ScaleToPoint(pxPoint, factor, 0.0);

  Invalidate();
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

int Engine::AddModelViewListener(TScreenChangedFn const & listener)
{
  return m_navigator.AddViewportListener(listener);
}

void Engine::RemoveModelViewListener(int slotID)
{
  m_navigator.RemoveViewportListener(slotID);
}

int Engine::GetDrawScale()
{
  return m_navigator.GetDrawScale();
}

void Engine::Touch(Engine::ETouchAction action, Engine::ETouchMask mask,
                   m2::PointD const & pt1, m2::PointD const & pt2)
{
  if ((mask != MASK_FIRST) || (action == ACTION_CANCEL))
  {
    if (mask == MASK_FIRST && m_guiController)
      m_guiController->OnTapCancelled(pt1);

    m_isCleanSingleClick = false;
    KillTouchTask();
  }
  else
  {
    ASSERT_EQUAL(mask, MASK_FIRST, ());

    if (action == ACTION_DOWN)
    {
      KillTouchTask();

      m_wasLongClick = false;
      m_isCleanSingleClick = true;
      m_lastTouch = pt1;

      if (m_guiController && m_guiController->OnTapStarted(pt1))
        return;

      StartTouchTask(pt1, LONG_TOUCH_MS);
    }

    if (action == ACTION_MOVE)
    {
      double const minDist = m_renderPolicy->VisualScale() * 10.0;
      if (m_lastTouch.SquareLength(pt1) > minDist * minDist)
      {
        m_isCleanSingleClick = false;
        KillTouchTask();
      }

      if (m_guiController && m_guiController->OnTapMoved(pt1))
        return;
    }

    if (action == ACTION_UP)
    {
      KillTouchTask();

      if (m_guiController && m_guiController->OnTapEnded(pt1))
        return;

      if (!m_wasLongClick && m_isCleanSingleClick)
      {
        if (m_doubleClickTimer.ElapsedSeconds() <= DOUBLE_TOUCH_S)
        {
          // performing double-click
          Scale(1.5 /* factor */, pt1, true);
        }
        else
        {
          // starting single touch task
          StartTouchTask(pt1, SHORT_TOUCH_MS);

          // starting double click
          m_doubleClickTimer.Reset();
        }
      }
      else
        m_wasLongClick = false;
    }
  }

  // general case processing
  if (m_mask != mask)
  {
    if (m_mask == MASK_EMPTY)
    {
      if (mask == MASK_FIRST)
        StartDrag(DragEvent(pt1));
      else if (mask == MASK_SECOND)
        StartDrag(DragEvent(pt1));
      else if (mask == MASK_BOTH)
        StartScale(ScaleEvent(pt1, pt2));
    }
    else if (m_mask == MASK_FIRST)
    {
      StopDrag(DragEvent(pt1));

      if (mask == MASK_EMPTY)
      {
        if ((action != ACTION_UP) && (action != ACTION_CANCEL))
          LOG(LWARNING, ("should be ACTION_UP or ACTION_CANCEL"));
      }
      else if (mask == MASK_SECOND)
        StartDrag(DragEvent(pt2));
      else if (mask == MASK_BOTH)
        StartScale(ScaleEvent(pt1, pt2));
    }
    else if (m_mask == MASK_SECOND)
    {
      StopDrag(DragEvent(pt2));

      if (mask == MASK_EMPTY)
      {
        if ((action != ACTION_UP) && (action != ACTION_CANCEL))
          LOG(LWARNING, ("should be ACTION_UP or ACTION_CANCEL"));
      }
      else if (mask == MASK_FIRST)
        StartDrag(DragEvent(pt1));
      else if (mask == MASK_BOTH)
        StartScale(ScaleEvent(pt1, pt2));
    }
    else if (m_mask == MASK_BOTH)
    {
      StopScale(ScaleEvent(m_touch1, m_touch2));

      if (action == ACTION_MOVE)
      {
        if (mask == MASK_FIRST)
          StartDrag(DragEvent(pt1));

        if (mask == MASK_SECOND)
          StartDrag(DragEvent(pt2));
      }
      else
        mask = MASK_EMPTY;
    }
  }
  else
  {
    if (action == ACTION_MOVE)
    {
      if (m_mask == MASK_FIRST)
        DoDrag(DragEvent(pt1));
      if (m_mask == MASK_SECOND)
        DoDrag(DragEvent(pt2));
      if (m_mask == MASK_BOTH)
        DoScale(ScaleEvent(pt1, pt2));
    }

    if ((action == ACTION_CANCEL) || (action == ACTION_UP))
    {
      if (m_mask == MASK_FIRST)
        StopDrag(DragEvent(pt1));
      if (m_mask == MASK_SECOND)
        StopDrag(DragEvent(pt2));
      if (m_mask == MASK_BOTH)
        StopScale(ScaleEvent(m_touch1, m_touch2));
      mask = MASK_EMPTY;
    }
  }

  m_touch1 = pt1;
  m_touch2 = pt2;
  m_mask = mask;
}

void Engine::StartDrag(DragEvent const & e)
{
  m_navigator.StartDrag(m_navigator.ShiftPoint(e.Pos()));
  //m_informationDisplay.locationState()->DragStarted();
  m_renderPolicy->StartDrag();
}

void Engine::DoDrag(DragEvent const & e)
{
  m_navigator.DoDrag(m_navigator.ShiftPoint(e.Pos()));
  m_renderPolicy->DoDrag();
}

void Engine::StopDrag(DragEvent const & e)
{
  m_navigator.StopDrag(m_navigator.ShiftPoint(e.Pos()));
  //m_informationDisplay.locationState()->DragEnded();
  m_renderPolicy->StopDrag();
}

void Engine::StartScale(ScaleEvent const & e)
{
  m2::PointD pt1, pt2;
  CalcScalePoints(e, pt1, pt2);

  //GetLocationState()->ScaleStarted();
  m_navigator.StartScale(pt1, pt2);
  m_renderPolicy->StartScale();
}

void Engine::DoScale(ScaleEvent const & e)
{
  m2::PointD pt1, pt2;
  CalcScalePoints(e, pt1, pt2);

  m_navigator.DoScale(pt1, pt2);
  m_renderPolicy->DoScale();
  //if (m_navigator.IsRotatingDuringScale())
  //  GetLocationState()->Rotated();
}

void Engine::StopScale(ScaleEvent const & e)
{
  m2::PointD pt1, pt2;
  CalcScalePoints(e, pt1, pt2);

  m_navigator.StopScale(pt1, pt2);
  m_renderPolicy->StopScale();

  //GetLocationState()->ScaleEnded();
}

void Engine::CalcScalePoints(ScaleEvent const & e, m2::PointD & pt1, m2::PointD & pt2) const
{
  pt1 = m_navigator.ShiftPoint(e.Pt1());
  pt2 = m_navigator.ShiftPoint(e.Pt2());

  //m_informationDisplay.locationState()->CorrectScalePoint(pt1, pt2);
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

void Engine::StartTouchTask(m2::PointD const & pt, unsigned ms)
{
  m_deferredTask.reset(new DeferredTask(bind(&Engine::OnProcessTouchTask, this, pt, ms),
                                        milliseconds(ms)));
}

void Engine::KillTouchTask()
{
  m_deferredTask.reset();
}

void Engine::OnProcessTouchTask(m2::PointD const & pt, unsigned ms)
{
  m_wasLongClick = (ms == LONG_TOUCH_MS);
  //GetPinClickManager().OnShowMark(m_work.GetUserMark(m2::PointD(x, y), m_wasLongClick));
}

}

