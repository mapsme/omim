#pragma once

#include "indexer/index.hpp"

#include "std/string.hpp"

namespace generator
{
namespace tests_support
{
/// \note This method could be used to check correctness of edge index section
/// of any mwm which has one.
void TestEdgeIndex(MwmValue const & mwmValue, MwmSet::MwmId const & mwmId,
                   string const & dir, string const & country);

}  // namespace tests_support
}  // namespace generator
