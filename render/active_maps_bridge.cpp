#include "active_maps_bridge.hpp"

namespace rg
{

void ActiveMapsBridge::SetStatusListener(TCountryStatusListener const & fn)
{
  m_statusListener = fn;
}

void ActiveMapsBridge::SetDownloadListener(TCountryDownloadListener const & fn)
{
  m_progressListener = fn;
}

void ActiveMapsBridge::ResetListeners()
{
  m_statusListener = TCountryStatusListener();
  m_progressListener = TCountryDownloadListener();
}

void ActiveMapsBridge::CallStatusListener(storage::TIndex const & index, storage::TStatus const & status)
{
  if (m_statusListener)
    m_statusListener(index, status);
}

void ActiveMapsBridge::CallDownloadListener(storage::TIndex const & index, storage::LocalAndRemoteSizeT const & sizes)
{
  if (m_progressListener)
    m_progressListener(index, sizes);
}

}
