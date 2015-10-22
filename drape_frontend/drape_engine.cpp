#include "drape_frontend/drape_engine.hpp"

#include "drape_frontend/message_subclasses.hpp"
#include "drape_frontend/visual_params.hpp"

#include "drape_gui/drape_gui.hpp"

#include "drape/texture_manager.hpp"

#include "std/bind.hpp"
#include "std/condition_variable.hpp"
#include "std/mutex.hpp"

namespace df
{

DrapeEngine::DrapeEngine(Params const & params)
  : m_viewport(params.m_viewport)
{
  VisualParams::Init(params.m_vs, df::CalculateTileSize(m_viewport.GetWidth(), m_viewport.GetHeight()));

  gui::DrapeGui::TScaleFactorFn scaleFn = []
  {
    return VisualParams::Instance().GetVisualScale();
  };
  gui::DrapeGui::TGeneralizationLevelFn gnLvlFn = [](ScreenBase const & screen)
  {
    return GetDrawTileScale(screen);
  };

  gui::DrapeGui & guiSubsystem = gui::DrapeGui::Instance();
  guiSubsystem.Init(scaleFn, gnLvlFn);
  guiSubsystem.SetLocalizator(bind(&StringsBundle::GetString, params.m_stringsBundle.GetRaw(), _1));
  guiSubsystem.SetStorageAccessor(params.m_storageAccessor);

  m_textureManager = dp::MasterPointer<dp::TextureManager>(new dp::TextureManager());
  m_threadCommutator = dp::MasterPointer<ThreadsCommutator>(new ThreadsCommutator());
  dp::RefPointer<ThreadsCommutator> commutatorRef = m_threadCommutator.GetRefPointer();

  m_frontend = dp::MasterPointer<FrontendRenderer>(new FrontendRenderer(commutatorRef,
                                                                        params.m_factory,
                                                                        m_textureManager.GetRefPointer(),
                                                                        m_viewport));
  m_backend =  dp::MasterPointer<BackendRenderer>(new BackendRenderer(commutatorRef,
                                                                      params.m_factory,
                                                                      m_textureManager.GetRefPointer(),
                                                                      params.m_model));
}

DrapeEngine::~DrapeEngine()
{
  m_frontend.Destroy();
  m_backend.Destroy();
  m_threadCommutator.Destroy();
  m_textureManager.Destroy();
}

void DrapeEngine::Resize(int w, int h)
{
  if (m_viewport.GetWidth() == w && m_viewport.GetHeight() == h)
    return;

  m_viewport.SetViewport(0, 0, w, h);
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  dp::MovePointer<Message>(new ResizeMessage(m_viewport)),
                                  MessagePriority::High);
}

void DrapeEngine::UpdateCoverage(ScreenBase const & screen)
{
  m_frontend->SetModelView(screen);
}

void DrapeEngine::ClearUserMarksLayer(df::TileKey const & tileKey)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  dp::MovePointer<Message>(new ClearUserMarkLayerMessage(tileKey)),
                                  MessagePriority::Normal);
}

void DrapeEngine::ChangeVisibilityUserMarksLayer(TileKey const & tileKey, bool isVisible)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  dp::MovePointer<Message>(new ChangeUserMarkLayerVisibilityMessage(tileKey, isVisible)),
                                  MessagePriority::Normal);
}

void DrapeEngine::UpdateUserMarksLayer(TileKey const & tileKey, UserMarksProvider * provider)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  dp::MovePointer<Message>(new UpdateUserMarkLayerMessage(tileKey, provider)),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetRenderingEnabled(bool const isEnabled)
{
  m_frontend->SetRenderingEnabled(isEnabled);
  m_backend->SetRenderingEnabled(isEnabled);

  LOG(LDEBUG, (isEnabled ? "Rendering enabled" : "Rendering disabled"));
}

} // namespace df
