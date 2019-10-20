#include "indexer/complex/complex_manager.hpp"

#include "indexer/complex/serdes.hpp"

#include "defines.hpp"

namespace indexer
{
ComplexManager::ComplexManager(DataSource const & dataSource) : m_dataSource(dataSource) {}

std::vector<FeatureID> ComplexManager::GetPath(FeatureID const & id) const
{
  std::vector<FeatureID> path;
  auto node = FindNode(id);
  if (!node)
    return path;

  while ((node = node->GetParent()))
    path.emplace_back(FeatureID(id.m_mwmId, node->GetData()));
  return path;
}

std::vector<FeatureID> ComplexManager::GetFamily(FeatureID const & id) const
{
  std::vector<FeatureID> family;
  auto const node = FindNode(id);
  if (!node)
    return family;

  auto const root = tree_node::GetRoot(node);
  tree_node::ForEach(root, [&](auto ftId) {
    family.emplace_back(FeatureID(id.m_mwmId, ftId));
  });
  return family;
}

tree_node::types::Ptr<complex::Id> ComplexManager::FindNode(FeatureID const & id) const
{
  auto const it = m_complexes.find(id.m_mwmId);
  if (it == std::cend(m_complexes))
    return nullptr;

  auto const & forest = it->second;
  return tree_node::FindIf(forest, [&](auto ftId) { return id.m_index == ftId; });
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
  tree_node::Forest<complex::Id> forest;
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
}  // namespace  indexer
