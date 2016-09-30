#include "testing/testing.hpp"

#include "generator/generator_tests_support/test_mwm_consistency.hpp"

#include "routing/features_road_graph.hpp"
#include "routing/pedestrian_model.hpp"
#include "routing/road_graph.hpp"

#include "indexer/edge_index_loader.hpp"
#include "indexer/feature_altitude.hpp"
#include "indexer/feature_processor.hpp"
#include "indexer/index.hpp"

#include "coding/file_name_utils.hpp"

#include "platform/country_file.hpp"

#include "std/unique_ptr.hpp"

using namespace feature;
using namespace platform;
using namespace routing;

namespace
{
double constexpr kEpsilon = 1e-5;

void SortEdges(IRoadGraph::TEdgeVector & edges1, IRoadGraph::TEdgeVector & edges2)
{
  sort(edges1.begin(), edges1.end());
  sort(edges2.begin(), edges2.end());
}

void TestEqualEdges(IRoadGraph::TEdgeVector const & edgesFormIndex,
                    IRoadGraph::TEdgeVector const & edgesFormGeometry)
{
  TEST_EQUAL(edgesFormIndex.size(), edgesFormGeometry.size(), ());
  for (size_t j = 0; j < edgesFormIndex.size(); ++j)
  {
    Edge const & fromIndex = edgesFormIndex[j];
    Edge const & fromGeom = edgesFormGeometry[j];
    TEST(m2::AlmostEqualAbs(fromIndex.GetStartJunction().GetPoint(),
                            fromGeom.GetStartJunction().GetPoint(), kEpsilon),
         (fromIndex.GetStartJunction().GetPoint(), fromGeom.GetStartJunction().GetPoint()));
    TEST(m2::AlmostEqualAbs(fromIndex.GetEndJunction().GetPoint(),
                            fromGeom.GetEndJunction().GetPoint(), kEpsilon),
         (fromIndex.GetEndJunction().GetPoint(), fromGeom.GetEndJunction().GetPoint()));
    TEST_EQUAL(fromIndex.IsForward(), fromGeom.IsForward(), ());
    TEST_EQUAL(fromIndex.GetFeatureId().m_index, fromGeom.GetFeatureId().m_index, ());
    TEST_EQUAL(fromIndex.GetSegId(), fromGeom.GetSegId(), ());
  }
}
}  // namespace

namespace generator
{
namespace tests_support
{
/// \note This method could be used to check correctness of edge index section
/// of any mwm which has one.
void TestEdgeIndex(MwmValue const & mwmValue, MwmSet::MwmId const & mwmId,
                   string const & dir, string const & country)
{
  feature::EdgeIndexLoader loader(mwmValue, mwmId);
  TEST(loader.HasEdgeIndex(), ());

  string const mwmPath = my::JoinFoldersToPath(dir, country + DATA_FILE_EXTENSION);
  Index index;
  LocalCountryFile localCountryFile(dir, CountryFile(country), 1 /* version */);
  index.RegisterMap(localCountryFile);
  FeaturesRoadGraph featureRoadGraph(index, IRoadGraph::Mode::IgnoreOnewayTag,
                                     make_unique<PedestrianModelFactory>());

  auto processor = [&loader, &featureRoadGraph](FeatureType const & f, uint32_t const & id)
  {
    f.ParseGeometry(FeatureType::BEST_GEOMETRY);
    size_t const pointsCount = f.GetPointsCount();
    for (size_t i = 0; i < pointsCount; ++i)
    {
      // Outgoing edges.
      {
        // Edges from edge index section.
        Junction const junction(f.GetPoint(i), kDefaultAltitudeMeters);
        IRoadGraph::TEdgeVector edgesFormIndex;
        loader.GetOutgoingEdges(junction, edgesFormIndex);

        // Edges from geometry.
        IRoadGraph::TEdgeVector edgesFormGeometry;
        featureRoadGraph.GetOutgoingEdges(junction, edgesFormGeometry);

        // Comparing outgoing edges for edge index section and from geometry.
        SortEdges(edgesFormIndex, edgesFormGeometry);
        TestEqualEdges(edgesFormIndex, edgesFormGeometry);
      }

      // Ingoing edges.
      {
        // Edges from edge index section.
        Junction const junction(f.GetPoint(i), kDefaultAltitudeMeters);
        IRoadGraph::TEdgeVector edgesFormIndex;
        loader.GetIngoingEdges(junction, edgesFormIndex);

        // Edges from geometry.
        IRoadGraph::TEdgeVector edgesFormGeometry;
        featureRoadGraph.GetIngoingEdges(junction, edgesFormGeometry);

        // Comparing ingoing edges for edge index section and from geometry.
        SortEdges(edgesFormIndex, edgesFormGeometry);
        TestEqualEdges(edgesFormIndex, edgesFormGeometry);
      }
    }
  };
  ForEachFromDat(mwmPath, processor);
}
}  // namespace tests_support
}  // namespace generator
