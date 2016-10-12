#pragma once

#include "std/string.hpp"

namespace generator
{
namespace tests_support
{
/// \brief Test if edge index section corresponds to geometry index in an mwm.
/// \param dir is a full path to directory where the map is located.
/// \param countryName is a map name without extention.
/// \note This test compares result of FeaturesRoadGraph::GetOutgoingEdges(GetIngoingEdges)
/// and EdgeIndexLoader::GetOutgoingEdges(GetIngoingEdges) methods.
/// It is implied that FeaturesRoadGraph::GetOutgoingEdges(GetIngoingEdges) methods
/// get edges based on geometry index.
void TestEdgeIndex(string const & dir, string const & countryName);

}  // namespace tests_support
}  // namespace generator
