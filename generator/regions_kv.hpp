#pragma once

#include <string>

namespace feature
{
struct GenerateInfo;
}  // namespace feature

namespace generator
{
bool GenerateRegionsKv(const feature::GenerateInfo & genInfo);
}  // namespace generator
