#pragma once

#include "std/set.hpp"
#include "std/string.hpp"
#include "std/vector.hpp"

class PublicTransportMetadata
{
public:
  static string Serialize(set<string> const & routes);
  static vector<string> Deserialize(string const & serializedRoutes);
};
