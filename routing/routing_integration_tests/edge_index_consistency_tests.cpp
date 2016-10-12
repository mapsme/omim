#include "testing/testing.hpp"

#include "routing/routing_integration_tests/routing_test_tools.hpp"

#include "generator/generator_tests_support/test_mwm_consistency.hpp"

#include "indexer/classificator.hpp"
#include "indexer/classificator_loader.hpp"

#include "platform/platform.hpp"

#include "std/unique_ptr.hpp"

namespace
{
using namespace integration;
using namespace generator;
using namespace generator::tests_support;

string const kTestMap = "Russia_Moscow";

// This test checks if edge index section corresponts to geometry index.
// That means if FeaturesRoadGraph::GetOutgoingEdges(...) and EdgeIndexLoader::GetOutgoingEdges(...)
// have the same result for real mwm.
// For the time being it's not true. For example for Russia_Moscow map in edge index section
// there's information to recover outgoing edges for 1070687 points. For 810 (of 1070687) points
// outgoing edges recovered from edge index and from geometry index are different.
// The reasons are described in edge_index_generator.cpp.
UNIT_TEST(EdgeIndexConsistency_MoscowTest)
{
  SetEnvironment();
  Platform & pl = GetPlatform();
  classificator::Load();
  classif().SortClassificator();

  TestEdgeIndex(pl.WritableDir(), kTestMap);
}
}  // namespace
