#include "indexer/public_transport_metadata.hpp"

#include "base/stl_add.hpp"
#include "base/string_utils.hpp"

namespace
{
char const kDelimiter[] = ";";
}  // namespace

string PublicTransportMetadata::Serialize(set<string> const & routes)
{
  return strings::JoinStrings(routes, kDelimiter);
}

vector<string> PublicTransportMetadata::Deserialize(string const & serializedRoutes)
{
  vector<string> routes;
  strings::Tokenize(serializedRoutes, kDelimiter, MakeBackInsertFunctor(routes));
  return routes;
}

