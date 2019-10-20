#pragma once

#include "indexer/complex/complex.hpp"
#include "indexer/complex/tree_node.hpp"
#include "indexer/data_source.hpp"
#include "indexer/feature_decl.hpp"
#include "indexer/mwm_set.hpp"

#include "platform/local_country_file.hpp"

#include <map>
#include <vector>

namespace indexer
{
class ComplexManager : public MwmSet::Observer
{
public:
  ComplexManager(DataSource const & dataSource);

  std::vector<FeatureID> GetPath(FeatureID const & id) const;
  std::vector<FeatureID> GetFamily(FeatureID const & id) const;

private:
  tree_node::types::Ptr<complex::Id> FindNode(FeatureID const & id) const;

  // MwmSet::Observer overrides:
  void OnMapRegistered(platform::LocalCountryFile const & localFile) override;
  void OnMapDeregistered(platform::LocalCountryFile const & localFile) override;

  DataSource const & m_dataSource;
  std::map<MwmSet::MwmId, tree_node::Forest<complex::Id>> m_complexes;
};
}  // namespace  indexer
