#include "drape_frontend/drape_engine.hpp"

#include "drape_frontend/message_subclasses.hpp"
#include "drape_frontend/gui/drape_gui.hpp"
#include "drape_frontend/my_position_controller.hpp"
#include "drape_frontend/visual_params.hpp"

#include "drape/support_manager.hpp"

#include "platform/settings.hpp"

using namespace std::placeholders;

namespace df
{
DrapeEngine::DrapeEngine(Params && params)
  : m_myPositionModeChanged(std::move(params.m_myPositionModeChanged))
  , m_viewport(std::move(params.m_viewport))
{
  VisualParams::Init(params.m_vs, df::CalculateTileSize(m_viewport.GetWidth(), m_viewport.GetHeight()));

  df::VisualParams::Instance().SetFontScale(params.m_fontsScaleFactor);

  gui::DrapeGui & guiSubsystem = gui::DrapeGui::Instance();
  guiSubsystem.SetLocalizator(std::bind(&StringsBundle::GetString, params.m_stringsBundle.get(), _1));
  guiSubsystem.SetSurfaceSize(m2::PointF(m_viewport.GetWidth(), m_viewport.GetHeight()));

  m_textureManager = make_unique_dp<dp::TextureManager>();
  m_threadCommutator = make_unique_dp<ThreadsCommutator>();
  m_requestedTiles = make_unique_dp<RequestedTiles>();

  location::EMyPositionMode mode = params.m_initialMyPositionMode.first;
  if (!params.m_initialMyPositionMode.second && !settings::Get(settings::kLocationStateMode, mode))
  {
    mode = location::PendingPosition;
  }
  else if (mode == location::FollowAndRotate)
  {
    // If the screen rect setting in follow and rotate mode is missing or invalid, it could cause
    // invalid animations, so the follow and rotate mode should be discarded.
    m2::AnyRectD rect;
    if (!(settings::Get("ScreenClipRect", rect) && df::GetWorldRect().IsRectInside(rect.GetGlobalRect())))
      mode = location::Follow;
  }

  double timeInBackground = 0.0;
  double lastEnterBackground = 0.0;
  if (settings::Get("LastEnterBackground", lastEnterBackground))
    timeInBackground = my::Timer::LocalTime() - lastEnterBackground;

  std::vector<PostprocessRenderer::Effect> effects;

  bool enabledAntialiasing;
  if (!settings::Get(dp::kSupportedAntialiasing, enabledAntialiasing))
    enabledAntialiasing = false;

  // Turn off AA for a while by energy-saving issues.
  //if (enabledAntialiasing)
  //{
  //  LOG(LINFO, ("Antialiasing is enabled"));
  //  effects.push_back(PostprocessRenderer::Antialiasing);
  //}

  MyPositionController::Params mpParams(mode,
                                        timeInBackground,
                                        params.m_hints,
                                        params.m_isRoutingActive,
                                        params.m_isAutozoomEnabled,
                                        std::bind(&DrapeEngine::MyPositionModeChanged, this, _1, _2));

  FrontendRenderer::Params frParams(params.m_apiVersion,
                                    make_ref(m_threadCommutator),
                                    params.m_factory,
                                    make_ref(m_textureManager),
                                    std::move(mpParams),
                                    m_viewport,
                                    std::bind(&DrapeEngine::ModelViewChanged, this, _1),
                                    std::bind(&DrapeEngine::TapEvent, this, _1),
                                    std::bind(&DrapeEngine::UserPositionChanged, this, _1, _2),
                                    make_ref(m_requestedTiles),
                                    std::move(params.m_overlaysShowStatsCallback),
                                    params.m_allow3dBuildings,
                                    params.m_trafficEnabled,
                                    params.m_blockTapEvents,
                                    std::move(effects));

  m_frontend = make_unique_dp<FrontendRenderer>(std::move(frParams));

  BackendRenderer::Params brParams(params.m_apiVersion,
                                   frParams.m_commutator,
                                   frParams.m_oglContextFactory,
                                   frParams.m_texMng,
                                   params.m_model,
                                   params.m_model.UpdateCurrentCountryFn(),
                                   make_ref(m_requestedTiles),
                                   params.m_allow3dBuildings,
                                   params.m_trafficEnabled,
                                   params.m_simplifiedTrafficColors);

  m_backend = make_unique_dp<BackendRenderer>(std::move(brParams));

  m_widgetsInfo = std::move(params.m_info);

  RecacheGui(false);
  RecacheMapShapes();

  if (params.m_showChoosePositionMark)
    EnableChoosePositionMode(true, std::move(params.m_boundAreaTriangles), false, m2::PointD());

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

void DrapeEngine::Update(int w, int h)
{
  if (m_choosePositionMode)
  {
    m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                    make_unique_dp<ShowChoosePositionMarkMessage>(),
                                    MessagePriority::Normal);
  }
  RecacheGui(false);

  RecacheMapShapes();

  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<RecoverGLResourcesMessage>(),
                                  MessagePriority::Normal);

  ResizeImpl(w, h);
}

void DrapeEngine::Resize(int w, int h)
{
  ASSERT_GREATER(w, 0, ());
  ASSERT_GREATER(h, 0, ());
  if (m_viewport.GetHeight() != static_cast<uint32_t>(h) ||
      m_viewport.GetWidth() != static_cast<uint32_t>(w))
    ResizeImpl(w, h);
}

void DrapeEngine::SetVisibleViewport(m2::RectD const & rect) const
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<SetVisibleViewportMessage>(rect),
                                  MessagePriority::Normal);
}

void DrapeEngine::Invalidate()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<InvalidateMessage>(),
                                  MessagePriority::High);
}

void DrapeEngine::AddTouchEvent(TouchEvent const & event)
{
  AddUserEvent(make_unique_dp<TouchEvent>(event));
}

void DrapeEngine::Scale(double factor, m2::PointD const & pxPoint, bool isAnim)
{
  AddUserEvent(make_unique_dp<ScaleEvent>(factor, pxPoint, isAnim));
}

void DrapeEngine::SetModelViewCenter(m2::PointD const & centerPt, int zoom, bool isAnim,
                                     bool trackVisibleViewport)
{
  PostUserEvent(make_unique_dp<SetCenterEvent>(centerPt, zoom, isAnim, trackVisibleViewport));
}

void DrapeEngine::SetModelViewRect(m2::RectD const & rect, bool applyRotation, int zoom, bool isAnim)
{
  PostUserEvent(make_unique_dp<SetRectEvent>(rect, applyRotation, zoom, isAnim));
}

void DrapeEngine::SetModelViewAnyRect(m2::AnyRectD const & rect, bool isAnim)
{
  PostUserEvent(make_unique_dp<SetAnyRectEvent>(rect, isAnim));
}

void DrapeEngine::ClearUserMarksGroup(size_t layerId)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<ClearUserMarkGroupMessage>(layerId),
                                  MessagePriority::Normal);
}

void DrapeEngine::ChangeVisibilityUserMarksGroup(MarkGroupID groupId, bool isVisible)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<ChangeUserMarkGroupVisibilityMessage>(groupId, isVisible),
                                  MessagePriority::Normal);
}

void DrapeEngine::InvalidateUserMarks()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<InvalidateUserMarksMessage>(),
                                  MessagePriority::Normal);
}

void DrapeEngine::UpdateUserMarksGroup(MarkGroupID groupId, UserMarksProvider * provider)
{
  auto groupIdCollection = make_unique_dp<MarkIDCollection>();
  auto removedIdCollection = make_unique_dp<MarkIDCollection>();
  auto createdIdCollection = make_unique_dp<MarkIDCollection>();

  auto marksRenderCollection = make_unique_dp<UserMarksRenderCollection>();
  marksRenderCollection->reserve(provider->GetUserPointCount());

  for (size_t pointIndex = 0, sz = provider->GetUserPointCount(); pointIndex < sz; ++pointIndex)
  {
    UserPointMark const * mark = provider->GetUserPointMark(pointIndex);
    groupIdCollection->m_marksID.push_back(mark->GetId());
    if (mark->IsDirty())
    {
      auto renderInfo = make_unique_dp<UserMarkRenderParams>();
      renderInfo->m_anchor = mark->GetAnchor();
      renderInfo->m_depth = mark->GetDepth();
      renderInfo->m_depthLayer = mark->GetDepthLayer();
      renderInfo->m_minZoom = mark->GetMinZoom();
      renderInfo->m_minTitleZoom = mark->GetMinTitleZoom();
      renderInfo->m_isVisible = mark->IsVisible();
      renderInfo->m_pivot = mark->GetPivot();
      renderInfo->m_pixelOffset = mark->GetPixelOffset();
      renderInfo->m_titleDecl = mark->GetTitleDecl();
      renderInfo->m_symbolNames = mark->GetSymbolNames();
      renderInfo->m_coloredSymbols = mark->GetColoredSymbols();
      renderInfo->m_symbolSizes = mark->GetSymbolSizes();
      renderInfo->m_hasSymbolPriority = mark->HasSymbolPriority();
      renderInfo->m_hasTitlePriority = mark->HasTitlePriority();
      renderInfo->m_priority = mark->GetPriority();
      renderInfo->m_index = mark->GetIndex();
      renderInfo->m_featureId = mark->GetFeatureID();
      renderInfo->m_hasCreationAnimation = mark->HasCreationAnimation();

      marksRenderCollection->emplace(mark->GetId(), std::move(renderInfo));
      mark->AcceptChanges();
    }
  }

  auto linesRenderCollection = make_unique_dp<UserLinesRenderCollection>();
  linesRenderCollection->reserve(provider->GetUserLineCount());
  for (size_t lineIndex = 0, sz = provider->GetUserLineCount(); lineIndex < sz; ++lineIndex)
  {
    UserLineMark const * mark = provider->GetUserLineMark(lineIndex);
    groupIdCollection->m_linesID.push_back(mark->GetId());
    if (mark->IsDirty())
    {
      auto renderInfo = make_unique_dp<UserLineRenderParams>();
      renderInfo->m_minZoom = mark->GetMinZoom();
      renderInfo->m_depthLayer = mark->GetDepthLayer();
      renderInfo->m_spline = m2::SharedSpline(mark->GetPoints());
      renderInfo->m_layers.reserve(mark->GetLayerCount());
      for (size_t layerIndex = 0, layersCount = mark->GetLayerCount(); layerIndex < layersCount; ++layerIndex)
      {
        renderInfo->m_layers.emplace_back(mark->GetColor(layerIndex),
                                          mark->GetWidth(layerIndex),
                                          mark->GetDepth(layerIndex));
      }

      linesRenderCollection->emplace(mark->GetId(), std::move(renderInfo));
      mark->AcceptChanges();
    }
  }

  provider->AcceptChanges(*createdIdCollection, *removedIdCollection);

  if (!createdIdCollection->IsEmpty() || !removedIdCollection->IsEmpty() ||
      !marksRenderCollection->empty() || !linesRenderCollection->empty())
  {
    m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                    make_unique_dp<UpdateUserMarksMessage>(
                                      std::move(createdIdCollection),
                                      std::move(removedIdCollection),
                                      std::move(marksRenderCollection),
                                      std::move(linesRenderCollection)),
                                    MessagePriority::Normal);
  }

  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<UpdateUserMarkGroupMessage>(
                                    groupId,
                                    std::move(groupIdCollection)),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetRenderingEnabled(ref_ptr<dp::OGLContextFactory> contextFactory)
{
  m_backend->SetRenderingEnabled(contextFactory);
  m_frontend->SetRenderingEnabled(contextFactory);

  LOG(LDEBUG, ("Rendering enabled"));
}

void DrapeEngine::SetRenderingDisabled(bool const destroyContext)
{
  m_frontend->SetRenderingDisabled(destroyContext);
  m_backend->SetRenderingDisabled(destroyContext);

  LOG(LDEBUG, ("Rendering disabled"));
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
  RecacheGui(false);
}

void DrapeEngine::RecacheMapShapes()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<MapShapesRecacheMessage>(),
                                  MessagePriority::Normal);
}

dp::DrapeID DrapeEngine::GenerateDrapeID()
{
  std::lock_guard<std::mutex> lock(m_drapeIdGeneratorMutex);
  ++m_drapeIdGenerator;
  return m_drapeIdGenerator;
}

void DrapeEngine::RecacheGui(bool needResetOldGui)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<GuiRecacheMessage>(m_widgetsInfo, needResetOldGui),
                                  MessagePriority::Normal);
}

void DrapeEngine::PostUserEvent(drape_ptr<UserEvent> && e)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<PostUserEventMessage>(std::move(e)),
                                  MessagePriority::Normal);
}

void DrapeEngine::AddUserEvent(drape_ptr<UserEvent> && e)
{
  m_frontend->AddUserEvent(std::move(e));
}

void DrapeEngine::ModelViewChanged(ScreenBase const & screen)
{
  if (m_modelViewChanged != nullptr)
    m_modelViewChanged(screen);
}

void DrapeEngine::MyPositionModeChanged(location::EMyPositionMode mode, bool routingActive)
{
  settings::Set(settings::kLocationStateMode, mode);
  if (m_myPositionModeChanged != nullptr)
    m_myPositionModeChanged(mode, routingActive);
}

void DrapeEngine::TapEvent(TapInfo const & tapInfo)
{
  if (m_tapListener != nullptr)
    m_tapListener(tapInfo);
}

void DrapeEngine::UserPositionChanged(m2::PointD const & position, bool hasPosition)
{
  if (m_userPositionChanged != nullptr)
    m_userPositionChanged(position, hasPosition);
}

void DrapeEngine::ResizeImpl(int w, int h)
{
  gui::DrapeGui::Instance().SetSurfaceSize(m2::PointF(w, h));
  m_viewport.SetViewport(0, 0, w, h);
  PostUserEvent(make_unique_dp<ResizeEvent>(w, h));
}

void DrapeEngine::SetCompassInfo(location::CompassInfo const & info)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<CompassInfoMessage>(info),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetGpsInfo(location::GpsInfo const & info, bool isNavigable,
                             const location::RouteMatchingInfo & routeInfo)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<GpsInfoMessage>(info, isNavigable, routeInfo),
                                  MessagePriority::Normal);
}

void DrapeEngine::SwitchMyPositionNextMode()
{
  using Mode = ChangeMyPositionModeMessage::EChangeType;
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<ChangeMyPositionModeMessage>(Mode::SwitchNextMode),
                                  MessagePriority::Normal);
}

void DrapeEngine::LoseLocation()
{
  using Mode = ChangeMyPositionModeMessage::EChangeType;
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<ChangeMyPositionModeMessage>(Mode::LoseLocation),
                                  MessagePriority::Normal);
}

void DrapeEngine::StopLocationFollow()
{
  using Mode = ChangeMyPositionModeMessage::EChangeType;
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<ChangeMyPositionModeMessage>(Mode::StopFollowing),
                                  MessagePriority::Normal);
}

void DrapeEngine::FollowRoute(int preferredZoomLevel, int preferredZoomLevel3d, bool enableAutoZoom)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<FollowRouteMessage>(preferredZoomLevel,
                                                                     preferredZoomLevel3d, enableAutoZoom),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetModelViewListener(TModelViewListenerFn && fn)
{
  m_modelViewChanged = std::move(fn);
}

void DrapeEngine::SetTapEventInfoListener(TTapEventInfoFn && fn)
{
  m_tapListener = std::move(fn);
}

void DrapeEngine::SetUserPositionListener(TUserPositionChangedFn && fn)
{
  m_userPositionChanged = std::move(fn);
}

void DrapeEngine::SelectObject(SelectionShape::ESelectedObject obj, m2::PointD const & pt,
                               FeatureID const & featureId, bool isAnim)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<SelectObjectMessage>(obj, pt, featureId, isAnim),
                                  MessagePriority::Normal);
}

void DrapeEngine::DeselectObject()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<SelectObjectMessage>(SelectObjectMessage::DismissTag()),
                                  MessagePriority::Normal);
}

dp::DrapeID DrapeEngine::AddSubroute(SubrouteConstPtr subroute)
{
  dp::DrapeID const id = GenerateDrapeID();
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<AddSubrouteMessage>(id, subroute),
                                  MessagePriority::Normal);
  return id;
}

void DrapeEngine::RemoveSubroute(dp::DrapeID subrouteId, bool deactivateFollowing)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<RemoveSubrouteMessage>(subrouteId, deactivateFollowing),
                                  MessagePriority::Normal);
}

void DrapeEngine::DeactivateRouteFollowing()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<DeactivateRouteFollowingMessage>(),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetSubrouteVisibility(dp::DrapeID subrouteId, bool isVisible)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<SetSubrouteVisibilityMessage>(subrouteId, isVisible),
                                  MessagePriority::Normal);
}

dp::DrapeID DrapeEngine::AddRoutePreviewSegment(m2::PointD const & startPt, m2::PointD const & finishPt)
{
  dp::DrapeID const id = GenerateDrapeID();
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<AddRoutePreviewSegmentMessage>(id, startPt, finishPt),
                                  MessagePriority::Normal);
  return id;
}

void DrapeEngine::RemoveRoutePreviewSegment(dp::DrapeID segmentId)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<RemoveRoutePreviewSegmentMessage>(segmentId),
                                  MessagePriority::Normal);
}

void DrapeEngine::RemoveAllRoutePreviewSegments()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<RemoveRoutePreviewSegmentMessage>(),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetWidgetLayout(gui::TWidgetsLayoutInfo && info)
{
  m_widgetsLayout = std::move(info);
  for (auto const & layout : m_widgetsLayout)
  {
    auto const itInfo = m_widgetsInfo.find(layout.first);
    if (itInfo != m_widgetsInfo.end())
      itInfo->second.m_pixelPivot = layout.second;
  }
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<GuiLayerLayoutMessage>(m_widgetsLayout),
                                  MessagePriority::Normal);
}

void DrapeEngine::AllowAutoZoom(bool allowAutoZoom)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<AllowAutoZoomMessage>(allowAutoZoom),
                                  MessagePriority::Normal);
}

void DrapeEngine::Allow3dMode(bool allowPerspectiveInNavigation, bool allow3dBuildings)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<Allow3dBuildingsMessage>(allow3dBuildings),
                                  MessagePriority::Normal);

  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<Allow3dModeMessage>(allowPerspectiveInNavigation,
                                                                     allow3dBuildings),
                                  MessagePriority::Normal);
}

void DrapeEngine::EnablePerspective()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<EnablePerspectiveMessage>(),
                                  MessagePriority::Normal);
}

void DrapeEngine::UpdateGpsTrackPoints(std::vector<df::GpsTrackPoint> && toAdd,
                                       std::vector<uint32_t> && toRemove)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<UpdateGpsTrackPointsMessage>(std::move(toAdd),
                                                                              std::move(toRemove)),
                                  MessagePriority::Normal);
}

void DrapeEngine::ClearGpsTrackPoints()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<ClearGpsTrackPointsMessage>(),
                                  MessagePriority::Normal);
}

void DrapeEngine::EnableChoosePositionMode(bool enable, std::vector<m2::TriangleD> && boundAreaTriangles,
                                           bool hasPosition, m2::PointD const & position)
{
  m_choosePositionMode = enable;
  bool kineticScroll = m_kineticScrollEnabled;
  if (enable)
  {
    StopLocationFollow();
    m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                    make_unique_dp<ShowChoosePositionMarkMessage>(),
                                    MessagePriority::Normal);
    kineticScroll = false;
  }
  else
  {
    RecacheGui(true);
  }
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<SetAddNewPlaceModeMessage>(enable, std::move(boundAreaTriangles),
                                                                            kineticScroll, hasPosition, position),
                                  MessagePriority::Normal);
}

void DrapeEngine::BlockTapEvents(bool block)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<BlockTapEventsMessage>(block),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetKineticScrollEnabled(bool enabled)
{
  m_kineticScrollEnabled = enabled;
  if (m_choosePositionMode)
    return;

  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<SetKineticScrollEnabledMessage>(m_kineticScrollEnabled),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetTimeInBackground(double time)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<SetTimeInBackgroundMessage>(time),
                                  MessagePriority::High);
}

void DrapeEngine::SetDisplacementMode(int mode)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<SetDisplacementModeMessage>(mode),
                                  MessagePriority::Normal);
}

void DrapeEngine::RequestSymbolsSize(std::vector<string> const & symbols,
                                     TRequestSymbolsSizeCallback const & callback)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<RequestSymbolsSizeMessage>(symbols, callback),
                                  MessagePriority::Normal);
}

void DrapeEngine::EnableTraffic(bool trafficEnabled)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<EnableTrafficMessage>(trafficEnabled),
                                  MessagePriority::Normal);
}

void DrapeEngine::UpdateTraffic(traffic::TrafficInfo const & info)
{
  if (info.GetColoring().empty())
    return;

#ifdef DEBUG
  for (auto const & segmentPair : info.GetColoring())
    ASSERT_NOT_EQUAL(segmentPair.second, traffic::SpeedGroup::Unknown, ());
#endif

  df::TrafficSegmentsColoring segmentsColoring;
  segmentsColoring.emplace(info.GetMwmId(), info.GetColoring());

  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<UpdateTrafficMessage>(std::move(segmentsColoring)),
                                  MessagePriority::Normal);
}

void DrapeEngine::ClearTrafficCache(MwmSet::MwmId const & mwmId)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<ClearTrafficDataMessage>(mwmId),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetSimplifiedTrafficColors(bool simplified)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<SetSimplifiedTrafficColorsMessage>(simplified),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetFontScaleFactor(double scaleFactor)
{
  double const kMinScaleFactor = 0.5;
  double const kMaxScaleFactor = 2.0;

  scaleFactor = my::clamp(scaleFactor, kMinScaleFactor, kMaxScaleFactor);

  VisualParams::Instance().SetFontScale(scaleFactor);
}

void DrapeEngine::RunScenario(ScenarioManager::ScenarioData && scenarioData,
                              ScenarioManager::ScenarioCallback const & onStartFn,
                              ScenarioManager::ScenarioCallback const & onFinishFn)
{
  auto const & manager = m_frontend->GetScenarioManager();
  if (manager != nullptr)
    manager->RunScenario(std::move(scenarioData), onStartFn, onFinishFn);
}

void DrapeEngine::SetCustomFeatures(std::set<FeatureID> && ids)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<SetCustomFeaturesMessage>(std::move(ids)),
                                  MessagePriority::Normal);
}

void DrapeEngine::RemoveCustomFeatures(MwmSet::MwmId const & mwmId)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<RemoveCustomFeaturesMessage>(mwmId),
                                  MessagePriority::Normal);
}

void DrapeEngine::RemoveAllCustomFeatures()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<RemoveCustomFeaturesMessage>(),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetPosteffectEnabled(PostprocessRenderer::Effect effect, bool enabled)
{
  if (effect == df::PostprocessRenderer::Antialiasing)
  {
    LOG(LINFO, ("Antialiasing is", (enabled ? "enabled" : "disabled")));
    settings::Set(dp::kSupportedAntialiasing, enabled);
  }

  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<SetPosteffectEnabledMessage>(effect, enabled),
                                  MessagePriority::Normal);
}

void DrapeEngine::RunFirstLaunchAnimation()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<RunFirstLaunchAnimationMessage>(),
                                  MessagePriority::Normal);
}
}  // namespace df
