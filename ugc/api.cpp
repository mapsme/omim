#include "ugc/api.hpp"

#include "indexer/feature.hpp"

#include "platform/platform.hpp"

#include <chrono>

using namespace std;
using namespace ugc;

namespace ugc
{
Api::Api(Index const & index, std::string const & filename)
  : m_storage(filename)
  , m_loader(index)
{
}

void Api::GetUGC(FeatureID const & id, UGCCallback callback)
{
  m_thread.Push([=] { GetUGCImpl(id, callback); });
}

void Api::SetUGCUpdate(FeatureID const & id, UGCUpdate const & ugc)
{
  m_thread.Push([=] { SetUGCUpdate(id, ugc); });
}

void Api::GetUGCImpl(FeatureID const & id, UGCCallback callback)
{
  if (!id.IsValid())
  {
    GetPlatform().RunOnGuiThread([callback] { callback({}, {}); });
    return;
  }

  auto const update = m_storage.GetUGCUpdate(id);
  auto const ugc = m_loader.GetUGC(id);

  GetPlatform().RunOnGuiThread([ugc, update, callback] { callback(ugc, update); });
}

void Api::SetUGCUpdateImpl(FeatureID const & id, UGCUpdate const & ugc)
{
  m_storage.SetUGCUpdate(id, ugc);
}
}  // namespace ugc
