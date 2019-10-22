#include "indexer/complex/complex_manager.hpp"

#include "indexer/complex/serdes.hpp"

#include "defines.hpp"

namespace indexer
{
ComplexManager::ComplexManager(DataSource const & dataSource) : m_dataSource(dataSource) {}

std::vector<ComplexManager::Ids> ComplexManager::GetPath(FeatureID const & id) const
{
  std::vector<Ids> path;
  auto node = FindNode(id);
  if (!node)
    return path;

  while ((node = node->GetParent()))
    path.emplace_back(MakeFeatureIDs(node->GetData(), id.m_mwmId));
  return path;
}

std::vector<ComplexManager::Ids> ComplexManager::GetFamily(FeatureID const & id) const
{
  std::vector<Ids> family;
  auto const node = FindNode(id);
  if (!node)
    return family;

  auto const root = tree_node::GetRoot(node);
  tree_node::ForEach(root, [&](auto const & ftIds) {
    family.emplace_back(MakeFeatureIDs(ftIds, id.m_mwmId));
  });
  return family;
}

tree_node::types::Ptr<complex::Ids> ComplexManager::FindNode(FeatureID const & id) const
{
  auto const it = m_complexes.find(id.m_mwmId);
  if (it == std::cend(m_complexes))
    return nullptr;

  auto const & forest = it->second;
  return tree_node::FindIf(forest, [&](auto const & ftIds) {
    auto const it = base::FindIf(ftIds, [&](auto ftId) {
      return id.m_index == ftId;
    });
    return it != std::cend(ftIds);
  });
}

void ComplexManager::OnMapRegistered(platform::LocalCountryFile const & localFile)
{
  auto const mwmId = m_dataSource.GetMwmIdByCountryFile(localFile.GetCountryFile());
  auto const handle = m_dataSource.GetMwmHandleById(mwmId);
  if (!handle.IsAlive())
    return;

  auto const & value = *handle.GetValue<MwmValue>();
  if (!value.m_cont.IsExist(COMPLEXES_FILE_TAG))
    return;

  auto readerPtr = value.m_cont.GetReader(COMPLEXES_FILE_TAG);
  tree_node::Forest<complex::Ids> forest;
  if (!ComplexSerdes::Deserialize(*readerPtr.GetPtr(), forest))
    return;

  LOG(LINFO, ("Complexes loaded for", localFile.GetCountryName()));
  m_complexes.emplace(mwmId, forest);
}

void ComplexManager::OnMapDeregistered(platform::LocalCountryFile const & localFile)
{
  auto const mwmId = m_dataSource.GetMwmIdByCountryFile(localFile.GetCountryFile());
  m_complexes.erase(mwmId);
}

ComplexManager::Ids MakeFeatureIDs(complex::Ids const & ids, MwmSet::MwmId const & mwmId)
{
  ComplexManager::Ids res;
  res.reserve(ids.size());
  base::Transform(ids, std::back_inserter(res), [&](auto id) { return FeatureID(mwmId, id); });
  return res;
}
}  // namespace  indexer
