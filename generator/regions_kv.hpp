#pragma once

#include <string>

namespace feature
{
struct GenerateInfo;
}  // namespace feature

namespace generator
{
bool GenerateRegionsKv(feature::GenerateInfo const & genInfo);
}  // namespace generator
