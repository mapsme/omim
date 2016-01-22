#include "drape_frontend/drape_engine.hpp"
#include "drape_frontend/message_subclasses.hpp"
#include "drape_frontend/visual_params.hpp"

#include "drape_frontend/gui/country_status_helper.hpp"
#include "drape_frontend/gui/drape_gui.hpp"

#include "drape/texture_manager.hpp"

#include "platform/platform.hpp"
#include "platform/settings.hpp"

#include "std/bind.hpp"

namespace df
{

namespace
{

void ConnectDownloadFn(gui::CountryStatusHelper::EButtonType buttonType, MapDataProvider::TDownloadFn downloadFn)
{
  gui::DrapeGui & guiSubsystem = gui::DrapeGui::Instance();
  guiSubsystem.ConnectOnButtonPressedHandler(buttonType, [downloadFn, &guiSubsystem]()
  {
    storage::TIndex countryIndex = guiSubsystem.GetCountryStatusHelper().GetCountryIndex();
    ASSERT(countryIndex != storage::TIndex::INVALID, ());
    if (downloadFn != nullptr)
      downloadFn(countryIndex);
  });
}

string const LocationStateMode = "LastLocationStateMode";

}

DrapeEngine::DrapeEngine(Params && params)
  : m_viewport(params.m_viewport)
{
  VisualParams::Init(params.m_vs, df::CalculateTileSize(m_viewport.GetWidth(), m_viewport.GetHeight()));

  gui::DrapeGui & guiSubsystem = gui::DrapeGui::Instance();
  guiSubsystem.SetLocalizator(bind(&StringsBundle::GetString, params.m_stringsBundle.get(), _1));
  guiSubsystem.SetSurfaceSize(m2::PointF(m_viewport.GetWidth(), m_viewport.GetHeight()));

  ConnectDownloadFn(gui::CountryStatusHelper::BUTTON_TYPE_MAP, params.m_model.GetDownloadMapHandler());
  ConnectDownloadFn(gui::CountryStatusHelper::BUTTON_TRY_AGAIN, params.m_model.GetDownloadRetryHandler());

  m_textureManager = make_unique_dp<dp::TextureManager>();
  m_threadCommutator = make_unique_dp<ThreadsCommutator>();
  m_requestedTiles = make_unique_dp<RequestedTiles>();

  location::EMyPositionMode mode = params.m_initialMyPositionMode.first;
  if (!params.m_initialMyPositionMode.second && !Settings::Get(LocationStateMode, mode))
    mode = location::MODE_FOLLOW;

  FrontendRenderer::Params frParams(make_ref(m_threadCommutator), params.m_factory,
                                    make_ref(m_textureManager), m_viewport,
                                    bind(&DrapeEngine::ModelViewChanged, this, _1),
                                    params.m_model.GetIsCountryLoadedFn(),
                                    bind(&DrapeEngine::TapEvent, this, _1, _2, _3, _4),
                                    bind(&DrapeEngine::UserPositionChanged, this, _1),
                                    bind(&DrapeEngine::MyPositionModeChanged, this, _1),
                                    mode, make_ref(m_requestedTiles), params.m_allow3dBuildings);

  m_frontend = make_unique_dp<FrontendRenderer>(frParams);

  BackendRenderer::Params brParams(frParams.m_commutator, frParams.m_oglContextFactory,
                                   frParams.m_texMng, params.m_model, make_ref(m_requestedTiles),
                                   params.m_allow3dBuildings);
  m_backend = make_unique_dp<BackendRenderer>(brParams);

  m_widgetsInfo = move(params.m_info);

  GuiRecacheMessage::Blocker blocker;
  drape_ptr<GuiRecacheMessage> message(new GuiRecacheMessage(blocker, m_widgetsInfo, m_widgetSizes));
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread, move(message), MessagePriority::High);
  blocker.Wait();

  ResizeImpl(m_viewport.GetWidth(), m_viewport.GetHeight());
}

DrapeEngine::~DrapeEngine()
{
  // Call Teardown explicitly! We must wait for threads completion.
  m_frontend->Teardown();
  m_backend->Teardown();

  // Reset thread commutator, it stores BaseRenderer pointers.
  m_threadCommutator.reset();

  // Reset pointers to FrontendRenderer and BackendRenderer.
  m_frontend.reset();
  m_backend.reset();

  gui::DrapeGui::Instance().Destroy();
  m_textureManager->Release();
}

void DrapeEngine::Resize(int w, int h)
{
  if (m_viewport.GetHeight() != h || m_viewport.GetWidth() != w)
    ResizeImpl(w, h);
}

void DrapeEngine::Invalidate()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<InvalidateMessage>(),
                                  MessagePriority::High);
}

void DrapeEngine::AddTouchEvent(TouchEvent const & event)
{
  AddUserEvent(event);
}

void DrapeEngine::Scale(double factor, m2::PointD const & pxPoint, bool isAnim)
{
  AddUserEvent(ScaleEvent(factor, pxPoint, isAnim));
}

void DrapeEngine::SetModelViewCenter(m2::PointD const & centerPt, int zoom, bool isAnim)
{
  AddUserEvent(SetCenterEvent(centerPt, zoom, isAnim));
}

void DrapeEngine::SetModelViewRect(m2::RectD const & rect, bool applyRotation, int zoom, bool isAnim)
{
  AddUserEvent(SetRectEvent(rect, applyRotation, zoom, isAnim));
}

void DrapeEngine::SetModelViewAnyRect(m2::AnyRectD const & rect, bool isAnim)
{
  AddUserEvent(SetAnyRectEvent(rect, isAnim));
}

int DrapeEngine::AddModelViewListener(TModelViewListenerFn const & listener)
{
  static int currentSlotID = 0;
  VERIFY(m_listeners.insert(make_pair(++currentSlotID, listener)).second, ());
  return currentSlotID;
}

void DrapeEngine::RemoveModeViewListener(int slotID)
{
  m_listeners.erase(slotID);
}

void DrapeEngine::ClearUserMarksLayer(df::TileKey const & tileKey)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<ClearUserMarkLayerMessage>(tileKey),
                                  MessagePriority::Normal);
}

void DrapeEngine::ChangeVisibilityUserMarksLayer(TileKey const & tileKey, bool isVisible)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<ChangeUserMarkLayerVisibilityMessage>(tileKey, isVisible),
                                  MessagePriority::Normal);
}

void DrapeEngine::UpdateUserMarksLayer(TileKey const & tileKey, UserMarksProvider * provider)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<UpdateUserMarkLayerMessage>(tileKey, provider),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetRenderingEnabled(bool const isEnabled)
{
  m_frontend->SetRenderingEnabled(isEnabled);
  m_backend->SetRenderingEnabled(isEnabled);

  LOG(LDEBUG, (isEnabled ? "Rendering enabled" : "Rendering disabled"));
}

void DrapeEngine::InvalidateRect(m2::RectD const & rect)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<InvalidateRectMessage>(rect),
                                  MessagePriority::High);
}

void DrapeEngine::UpdateMapStyle()
{
  // Update map style.
  {
    UpdateMapStyleMessage::Blocker blocker;
    m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                    make_unique_dp<UpdateMapStyleMessage>(blocker),
                                    MessagePriority::High);
    blocker.Wait();
  }

  // Recache gui after updating of style.
  {
    GuiRecacheMessage::Blocker blocker;
    drape_ptr<GuiRecacheMessage> message(new GuiRecacheMessage(blocker, m_widgetsInfo, m_widgetSizes));
    m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread, move(message), MessagePriority::High);
    blocker.Wait();

    m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                    make_unique_dp<GuiLayerLayoutMessage>(m_widgetsLayout),
                                    MessagePriority::High);
  }
}

void DrapeEngine::AddUserEvent(UserEvent const & e)
{
  m_frontend->AddUserEvent(e);
}

void DrapeEngine::ModelViewChanged(ScreenBase const & screen)
{
  Platform & pl = GetPlatform();
  pl.RunOnGuiThread(bind(&DrapeEngine::ModelViewChangedGuiThread, this, screen));
}

void DrapeEngine::ModelViewChangedGuiThread(ScreenBase const & screen)
{
  for (pair<int, TModelViewListenerFn> const & p : m_listeners)
    p.second(screen);
}

void DrapeEngine::MyPositionModeChanged(location::EMyPositionMode mode)
{
  Settings::Set(LocationStateMode, mode);
  GetPlatform().RunOnGuiThread([this, mode]()
  {
    if (m_myPositionModeChanged != nullptr)
      m_myPositionModeChanged(mode);
  });
}

void DrapeEngine::TapEvent(m2::PointD const & pxPoint, bool isLong, bool isMyPosition, FeatureID const & feature)
{
  GetPlatform().RunOnGuiThread([=]()
  {
    if (m_tapListener)
      m_tapListener(pxPoint, isLong, isMyPosition, feature);
  });
}

void DrapeEngine::UserPositionChanged(m2::PointD const & position)
{
  GetPlatform().RunOnGuiThread([this, position]()
  {
    if (m_userPositionChangedFn)
      m_userPositionChangedFn(position);
  });
}

void DrapeEngine::ResizeImpl(int w, int h)
{
  gui::DrapeGui::Instance().SetSurfaceSize(m2::PointF(w, h));
  m_viewport.SetViewport(0, 0, w, h);
  AddUserEvent(ResizeEvent(w, h));
}

void DrapeEngine::SetCountryInfo(gui::CountryInfo const & info, bool isCurrentCountry)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<CountryInfoUpdateMessage>(info, isCurrentCountry),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetInvalidCountryInfo()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<CountryInfoUpdateMessage>(),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetCompassInfo(location::CompassInfo const & info)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<CompassInfoMessage>(info),
                                  MessagePriority::High);
}

void DrapeEngine::SetGpsInfo(location::GpsInfo const & info, bool isNavigable, const location::RouteMatchingInfo & routeInfo)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<GpsInfoMessage>(info, isNavigable, routeInfo),
                                  MessagePriority::High);
}

void DrapeEngine::MyPositionNextMode()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<ChangeMyPositionModeMessage>(ChangeMyPositionModeMessage::TYPE_NEXT),
                                  MessagePriority::High);
}

void DrapeEngine::FollowRoute(int preferredZoomLevel, int preferredZoomLevel3d, double rotationAngle, double angleFOV)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<FollowRouteMessage>(preferredZoomLevel, preferredZoomLevel3d,
                                                                     rotationAngle, angleFOV),
                                  MessagePriority::High);
}

void DrapeEngine::CancelMyPosition()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<ChangeMyPositionModeMessage>(ChangeMyPositionModeMessage::TYPE_CANCEL),
                                  MessagePriority::High);
}

void DrapeEngine::StopLocationFollow()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<ChangeMyPositionModeMessage>(ChangeMyPositionModeMessage::TYPE_STOP_FOLLOW),
                                  MessagePriority::High);
}

void DrapeEngine::InvalidateMyPosition()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<ChangeMyPositionModeMessage>(ChangeMyPositionModeMessage::TYPE_INVALIDATE),
                                  MessagePriority::High);
}

void DrapeEngine::SetMyPositionModeListener(location::TMyPositionModeChanged const & fn)
{
  m_myPositionModeChanged = fn;
}

void DrapeEngine::SetTapEventInfoListener(TTapEventInfoFn const & fn)
{
  m_tapListener = fn;
}

void DrapeEngine::SetUserPositionListener(DrapeEngine::TUserPositionChangedFn const & fn)
{
  m_userPositionChangedFn = fn;
}

FeatureID DrapeEngine::GetVisiblePOI(m2::PointD const & glbPoint)
{
  FeatureID result;
  BaseBlockingMessage::Blocker blocker;
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<FindVisiblePOIMessage>(blocker, glbPoint, result),
                                  MessagePriority::High);

  blocker.Wait();
  return result;
}

void DrapeEngine::SelectObject(SelectionShape::ESelectedObject obj, m2::PointD const & pt, bool isAnim)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<SelectObjectMessage>(obj, pt, isAnim),
                                  MessagePriority::High);
}

void DrapeEngine::DeselectObject()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<SelectObjectMessage>(SelectObjectMessage::DismissTag()),
                                  MessagePriority::High);
}

SelectionShape::ESelectedObject DrapeEngine::GetSelectedObject()
{
  SelectionShape::ESelectedObject object;
  BaseBlockingMessage::Blocker blocker;
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<GetSelectedObjectMessage>(blocker, object),
                                  MessagePriority::High);

  blocker.Wait();
  return object;
}

bool DrapeEngine::GetMyPosition(m2::PointD & myPosition)
{
  bool hasPosition = false;
  BaseBlockingMessage::Blocker blocker;
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<GetMyPositionMessage>(blocker, hasPosition, myPosition),
                                  MessagePriority::High);

  blocker.Wait();
  return hasPosition;
}

void DrapeEngine::AddRoute(m2::PolylineD const & routePolyline, vector<double> const & turns,
                           df::ColorConstant color)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<AddRouteMessage>(routePolyline, turns, color),
                                  MessagePriority::Normal);
}

void DrapeEngine::RemoveRoute(bool deactivateFollowing)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<RemoveRouteMessage>(deactivateFollowing),
                                  MessagePriority::Normal);
}

void DrapeEngine::DeactivateRouteFollowing()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<DeactivateRouteFollowingMessage>(),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetRoutePoint(m2::PointD const & position, bool isStart, bool isValid)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<CacheRouteSignMessage>(position, isStart, isValid),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetWidgetLayout(gui::TWidgetsLayoutInfo && info)
{
  m_widgetsLayout = move(info);
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<GuiLayerLayoutMessage>(m_widgetsLayout),
                                  MessagePriority::Normal);
}

gui::TWidgetsSizeInfo const & DrapeEngine::GetWidgetSizes()
{
  return m_widgetSizes;
}

void DrapeEngine::Allow3dMode(bool allowPerspectiveInNavigation, bool allow3dBuildings, double rotationAngle, double angleFOV)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<Allow3dBuildingsMessage>(allow3dBuildings),
                                  MessagePriority::Normal);

  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<Allow3dModeMessage>(allowPerspectiveInNavigation, allow3dBuildings,
                                                                     rotationAngle, angleFOV),
                                  MessagePriority::Normal);
}

void DrapeEngine::EnablePerspective(double rotationAngle, double angleFOV)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<EnablePerspectiveMessage>(rotationAngle, angleFOV),
                                  MessagePriority::Normal);
}

void DrapeEngine::UpdateGpsTrackPoints(vector<df::GpsTrackPoint> && toAdd, vector<uint32_t> && toRemove)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<UpdateGpsTrackPointsMessage>(move(toAdd), move(toRemove)),
                                  MessagePriority::Normal);
}

void DrapeEngine::ClearGpsTrackPoints()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<ClearGpsTrackPointsMessage>(),
                                  MessagePriority::Normal);
}

} // namespace df
