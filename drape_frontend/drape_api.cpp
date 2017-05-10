#include "drape_frontend/drape_api.hpp"
#include "drape_frontend/drape_engine.hpp"
#include "drape_frontend/message_subclasses.hpp"

namespace df
{

void DrapeApi::SetEngine(ref_ptr<DrapeEngine> engine)
{
  m_engine = engine;
}

void DrapeApi::AddLine(std::string const & id, DrapeApiLineData const & data)
{
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_engine == nullptr)
    return;

  auto it = m_lines.find(id);
  if (it != m_lines.end())
  {
    m_engine->m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                              make_unique_dp<DrapeApiRemoveMessage>(id),
                                              MessagePriority::Normal);
  }

  m_lines[id] = data;

  TLines lines;
  lines.insert(make_pair(id, data));
  m_engine->m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                            make_unique_dp<DrapeApiAddLinesMessage>(lines),
                                            MessagePriority::Normal);
}

void DrapeApi::RemoveLine(std::string const & id)
{
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_engine == nullptr)
    return;

  m_lines.erase(id);
  m_engine->m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                            make_unique_dp<DrapeApiRemoveMessage>(id),
                                            MessagePriority::Normal);
}

void DrapeApi::Clear()
{
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_engine == nullptr)
    return;

  m_lines.clear();
  m_engine->m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                            make_unique_dp<DrapeApiRemoveMessage>("", true /* remove all */),
                                            MessagePriority::Normal);
}

void DrapeApi::Invalidate()
{
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_engine == nullptr)
    return;

  m_engine->m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                            make_unique_dp<DrapeApiRemoveMessage>("", true /* remove all */),
                                            MessagePriority::Normal);

  m_engine->m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                            make_unique_dp<DrapeApiAddLinesMessage>(m_lines),
                                            MessagePriority::Normal);
}

} // namespace df
