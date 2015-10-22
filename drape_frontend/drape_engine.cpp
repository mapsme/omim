#include "drape_frontend/drape_engine.hpp"

#include "drape_frontend/message_subclasses.hpp"
#include "drape_frontend/visual_params.hpp"

#include "drape/texture_manager.hpp"

#include "std/bind.hpp"

namespace df
{

DrapeEngine::DrapeEngine(dp::RefPointer<dp::OGLContextFactory> contextfactory,
                         Viewport const & viewport,
                         MapDataProvider const & model)
  : m_viewport(viewport)
{
  GLFunctions::Init();
  VisualParams::Init(viewport.GetPixelRatio(), df::CalculateTileSize(m_viewport.GetWidth(), m_viewport.GetHeight()));

  m_threadCommutator = dp::MasterPointer<ThreadsCommutator>(new ThreadsCommutator());
  dp::RefPointer<ThreadsCommutator> commutatorRef = m_threadCommutator.GetRefPointer();

  m_frontend = dp::MasterPointer<FrontendRenderer>(new FrontendRenderer(commutatorRef,
                                                                        contextfactory,
                                                                        m_viewport));
  m_backend =  dp::MasterPointer<BackendRenderer>(new BackendRenderer(commutatorRef,
                                                                      contextfactory,
                                                                      model));
}

DrapeEngine::~DrapeEngine()
{
  m_frontend.Destroy();
  m_backend.Destroy();
  m_threadCommutator.Destroy();
}

void DrapeEngine::Resize(int w, int h)
{
  if (m_viewport.GetLogicWidth() == w && m_viewport.GetLogicHeight() == h)
    return;

  m_viewport.SetViewport(0, 0, w, h);
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  dp::MovePointer<Message>(new ResizeMessage(m_viewport)));
}

void DrapeEngine::UpdateCoverage(ScreenBase const & screen)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  dp::MovePointer<Message>(new UpdateModelViewMessage(screen)));
}

void DrapeEngine::ClearUserMarksLayer(df::TileKey const & tileKey)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  dp::MovePointer<Message>(new ClearUserMarkLayerMessage(tileKey)));
}

void DrapeEngine::ChangeVisibilityUserMarksLayer(TileKey const & tileKey, bool isVisible)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  dp::MovePointer<Message>(new ChangeUserMarkLayerVisibilityMessage(tileKey, isVisible)));
}

void DrapeEngine::UpdateUserMarksLayer(TileKey const & tileKey, UserMarksProvider * provider)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  dp::MovePointer<Message>(new UpdateUserMarkLayerMessage(tileKey, provider)));
}

} // namespace df
