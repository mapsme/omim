#pragma once

#include "ugc/types.hpp"

#include <map>
#include <string>

struct FeatureID;

namespace ugc
{
class Storage
{
public:
  explicit Storage(std::string const & filename);

  void GetUGCUpdate(FeatureID const & id, UGCUpdate & ugc) const;
  void SetUGCUpdate(FeatureID const & id, UGCUpdate const & ugc);

  void Save();
  void Load();

private:
  std::string m_filename;
  std::map<FeatureID, UGCUpdate> m_ugc;
};
}  // namespace ugc
