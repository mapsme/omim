#include "drape_frontend/gui/drape_gui.hpp"

#include "drape_frontend/backend_renderer.hpp"
#include "drape_frontend/batchers_pool.hpp"
#include "drape_frontend/gps_track_shape.hpp"
#include "drape_frontend/map_shape.hpp"
#include "drape_frontend/message_subclasses.hpp"
#include "drape_frontend/read_manager.hpp"
#include "drape_frontend/route_builder.hpp"
#include "drape_frontend/user_mark_shapes.hpp"
#include "drape_frontend/visual_params.hpp"

#include "indexer/scales.hpp"

#include "drape/texture_manager.hpp"

#include "platform/platform.hpp"

#include "base/logging.hpp"

#include "std/bind.hpp"

namespace df
{

BackendRenderer::BackendRenderer(Params const & params)
  : BaseRenderer(ThreadsCommutator::ResourceUploadThread, params)
  , m_model(params.m_model)
  , m_readManager(make_unique_dp<ReadManager>(params.m_commutator, m_model, params.m_allow3dBuildings))
  , m_requestedTiles(params.m_requestedTiles)
{
#ifdef DEBUG
  m_isTeardowned = false;
#endif

  gui::DrapeGui::Instance().SetRecacheCountryStatusSlot([this]()
  {
    m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                              make_unique_dp<CountryStatusRecacheMessage>(),
                              MessagePriority::High);
  });

  m_routeBuilder = make_unique_dp<RouteBuilder>([this](drape_ptr<RouteData> && routeData)
  {
    m_commutator->PostMessage(ThreadsCommutator::RenderThread,
                              make_unique_dp<FlushRouteMessage>(move(routeData)),
                              MessagePriority::Normal);
  }, [this](drape_ptr<RouteSignData> && routeSignData)
  {
    m_commutator->PostMessage(ThreadsCommutator::RenderThread,
                              make_unique_dp<FlushRouteSignMessage>(move(routeSignData)),
                              MessagePriority::Normal);
  });

  StartThread();
}

BackendRenderer::~BackendRenderer()
{
  ASSERT(m_isTeardowned, ());
}

void BackendRenderer::Teardown()
{
  gui::DrapeGui::Instance().ClearRecacheCountryStatusSlot();
  StopThread();
#ifdef DEBUG
  m_isTeardowned = true;
#endif
}

unique_ptr<threads::IRoutine> BackendRenderer::CreateRoutine()
{
  return make_unique<Routine>(*this);
}

void BackendRenderer::RecacheGui(gui::TWidgetsInitInfo const & initInfo, gui::TWidgetsSizeInfo & sizeInfo)
{
  drape_ptr<gui::LayerRenderer> layerRenderer = m_guiCacher.RecacheWidgets(initInfo, sizeInfo, m_texMng);
  drape_ptr<Message> outputMsg = make_unique_dp<GuiLayerRecachedMessage>(move(layerRenderer));
  m_commutator->PostMessage(ThreadsCommutator::RenderThread, move(outputMsg), MessagePriority::Normal);
}

void BackendRenderer::RecacheCountryStatus()
{
  drape_ptr<gui::LayerRenderer> layerRenderer = m_guiCacher.RecacheCountryStatus(m_texMng);
  drape_ptr<Message> outputMsg = make_unique_dp<GuiLayerRecachedMessage>(move(layerRenderer));
  m_commutator->PostMessage(ThreadsCommutator::RenderThread, move(outputMsg), MessagePriority::Normal);
}

void BackendRenderer::AcceptMessage(ref_ptr<Message> message)
{
  switch (message->GetType())
  {
  case Message::UpdateReadManager:
    {
      TTilesCollection tiles = m_requestedTiles->GetTiles();
      if (!tiles.empty())
      {
        ScreenBase const screen = m_requestedTiles->GetScreen();
        bool const is3dBuildings = m_requestedTiles->Is3dBuildings();
        m_readManager->UpdateCoverage(screen, is3dBuildings, tiles, m_texMng);

        gui::CountryStatusHelper & helper = gui::DrapeGui::Instance().GetCountryStatusHelper();
        if ((*tiles.begin()).m_zoomLevel > scales::GetUpperWorldScale())
          m_model.UpdateCountryIndex(helper.GetCountryIndex(), screen.ClipRect().Center());
        else
          helper.Clear();
      }
      break;
    }
  case Message::InvalidateReadManagerRect:
    {
      ref_ptr<InvalidateReadManagerRectMessage> msg = message;
      if (msg->NeedInvalidateAll())
        m_readManager->InvalidateAll();
      else
        m_readManager->Invalidate(msg->GetTilesForInvalidate());
      break;
    }
  case Message::CountryStatusRecache:
    {
      RecacheCountryStatus();
      break;
    }
  case Message::GuiRecache:
    {
      ref_ptr<GuiRecacheMessage> msg = message;
      RecacheGui(msg->GetInitInfo(), msg->GetSizeInfoMap());
      break;
    }
  case Message::GuiLayerLayout:
    {
      ref_ptr<GuiLayerLayoutMessage> msg = message;
      m_commutator->PostMessage(ThreadsCommutator::RenderThread,
                                make_unique_dp<GuiLayerLayoutMessage>(msg->AcceptLayoutInfo()),
                                MessagePriority::Normal);
      RecacheCountryStatus();
      break;
    }
  case Message::TileReadStarted:
    {
      m_batchersPool->ReserveBatcher(static_cast<ref_ptr<BaseTileMessage>>(message)->GetKey());
      break;
    }
  case Message::TileReadEnded:
    {
      ref_ptr<TileReadEndMessage> msg = message;
      m_batchersPool->ReleaseBatcher(msg->GetKey());
      break;
    }
  case Message::FinishReading:
    {
      ref_ptr<FinishReadingMessage> msg = message;
      m_commutator->PostMessage(ThreadsCommutator::RenderThread,
                                make_unique_dp<FinishReadingMessage>(move(msg->MoveTiles())),
                                MessagePriority::Normal);
      break;
    }
  case Message::MapShapeReaded:
    {
      ref_ptr<MapShapeReadedMessage> msg = message;
      auto const & tileKey = msg->GetKey();
      if (m_requestedTiles->CheckTileKey(tileKey) && m_readManager->CheckTileKey(tileKey))
      {
        ref_ptr<dp::Batcher> batcher = m_batchersPool->GetTileBatcher(tileKey);
        for (drape_ptr<MapShape> const & shape : msg->GetShapes())
          shape->Draw(batcher, m_texMng);
      }
      break;
    }
  case Message::UpdateUserMarkLayer:
    {
      ref_ptr<UpdateUserMarkLayerMessage> msg = message;
      TileKey const & key = msg->GetKey();

      UserMarksProvider const * marksProvider = msg->StartProcess();
      if (marksProvider->IsDirty())
      {
        m_commutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<ClearUserMarkLayerMessage>(key),
                                  MessagePriority::Normal);

        m_batchersPool->ReserveBatcher(key);
        CacheUserMarks(marksProvider, m_batchersPool->GetTileBatcher(key), m_texMng);
        m_batchersPool->ReleaseBatcher(key);
      }
      msg->EndProcess();
      break;
    }
  case Message::CountryInfoUpdate:
    {
      ref_ptr<CountryInfoUpdateMessage> msg = message;
      gui::CountryStatusHelper & helper = gui::DrapeGui::Instance().GetCountryStatusHelper();
      if (!msg->NeedShow())
      {
        // Country is already loaded, so there is no need to show status GUI
        // even if this country is updating.
        helper.Clear();
      }
      else
      {
        gui::CountryInfo const & info = msg->GetCountryInfo();
        if (msg->IsCurrentCountry() || helper.GetCountryIndex() == info.m_countryIndex)
        {
          helper.SetCountryInfo(info);
        }
      }
      break;
    }
  case Message::AddRoute:
    {
      ref_ptr<AddRouteMessage> msg = message;
      m_routeBuilder->Build(msg->GetRoutePolyline(), msg->GetTurns(), msg->GetColor(), m_texMng);
      break;
    }
  case Message::CacheRouteSign:
    {
      ref_ptr<CacheRouteSignMessage> msg = message;
      m_routeBuilder->BuildSign(msg->GetPosition(), msg->IsStart(), msg->IsValid(), m_texMng);
      break;
    }
  case Message::RemoveRoute:
    {
      ref_ptr<RemoveRouteMessage> msg = message;

      // we have to resend the message to FR, because it guaranties that
      // RemoveRouteMessage will be precessed after FlushRouteMessage
      m_commutator->PostMessage(ThreadsCommutator::RenderThread,
                                make_unique_dp<RemoveRouteMessage>(msg->NeedDeactivateFollowing()),
                                MessagePriority::Normal);
      break;
    }
  case Message::InvalidateTextures:
    {
      m_texMng->Invalidate(VisualParams::Instance().GetResourcePostfix());
      RecacheMyPosition();
      break;
    }
  case Message::CacheGpsTrackPoints:
    {
      ref_ptr<CacheGpsTrackPointsMessage> msg = message;
      drape_ptr<GpsTrackRenderData> data = make_unique_dp<GpsTrackRenderData>();
      data->m_pointsCount = msg->GetPointsCount();
      GpsTrackShape::Draw(m_texMng, *data.get());
      m_commutator->PostMessage(ThreadsCommutator::RenderThread,
                                make_unique_dp<FlushGpsTrackPointsMessage>(move(data)),
                                MessagePriority::Normal);
      break;
    }
  case Message::StopRendering:
    {
      ProcessStopRenderingMessage();
      break;
    }
  case Message::Allow3dBuildings:
    {
      ref_ptr<Allow3dBuildingsMessage> msg = message;
      m_readManager->Allow3dBuildings(msg->Allow3dBuildings());
      break;
    }
  default:
    ASSERT(false, ());
    break;
  }
}

void BackendRenderer::ReleaseResources()
{
  m_readManager->Stop();

  m_readManager.reset();
  m_batchersPool.reset();
  m_routeBuilder.reset();

  m_texMng->Release();
  m_contextFactory->getResourcesUploadContext()->doneCurrent();
}

BackendRenderer::Routine::Routine(BackendRenderer & renderer) : m_renderer(renderer) {}

void BackendRenderer::Routine::Do()
{
  m_renderer.m_contextFactory->getResourcesUploadContext()->makeCurrent();
  GLFunctions::Init();

  m_renderer.InitGLDependentResource();

  while (!IsCancelled())
  {
    m_renderer.ProcessSingleMessage();
    m_renderer.CheckRenderingEnabled();
  }

  m_renderer.ReleaseResources();
}

void BackendRenderer::InitGLDependentResource()
{
  m_batchersPool = make_unique_dp<BatchersPool>(ReadManager::ReadCount(), bind(&BackendRenderer::FlushGeometry, this, _1));

  dp::TextureManager::Params params;
  params.m_resPostfix = VisualParams::Instance().GetResourcePostfix();
  params.m_visualScale = df::VisualParams::Instance().GetVisualScale();
  params.m_colors = "colors.txt";
  params.m_patterns = "patterns.txt";
  params.m_glyphMngParams.m_uniBlocks = "unicode_blocks.txt";
  params.m_glyphMngParams.m_whitelist = "fonts_whitelist.txt";
  params.m_glyphMngParams.m_blacklist = "fonts_blacklist.txt";
  params.m_glyphMngParams.m_sdfScale = VisualParams::Instance().GetGlyphSdfScale();
  params.m_glyphMngParams.m_baseGlyphHeight = VisualParams::Instance().GetGlyphBaseSize();
  GetPlatform().GetFontNames(params.m_glyphMngParams.m_fonts);

  m_texMng->Init(params);

  RecacheMyPosition();
}

void BackendRenderer::RecacheMyPosition()
{
  auto msg = make_unique_dp<MyPositionShapeMessage>(make_unique_dp<MyPosition>(m_texMng),
                                                    make_unique_dp<SelectionShape>(m_texMng));

  GLFunctions::glFlush();
  m_commutator->PostMessage(ThreadsCommutator::RenderThread, move(msg), MessagePriority::High);
}

void BackendRenderer::FlushGeometry(drape_ptr<Message> && message)
{
  GLFunctions::glFlush();
  m_commutator->PostMessage(ThreadsCommutator::RenderThread, move(message), MessagePriority::Normal);
}

} // namespace df
