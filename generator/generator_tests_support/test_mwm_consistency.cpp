#include "testing/testing.hpp"

#include "generator/generator_tests_support/test_mwm_consistency.hpp"

#include "routing/features_road_graph.hpp"
#include "routing/routing_helpers.hpp"
#include "routing/pedestrian_model.hpp"
#include "routing/road_graph.hpp"

#include "indexer/edge_index_loader.hpp"
#include "indexer/feature_altitude.hpp"
#include "indexer/feature_data.hpp"
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
double constexpr kEpsilon = 1e-4;

void SortEdges(IRoadGraph::TEdgeVector & edges1, IRoadGraph::TEdgeVector & edges2)
{
  auto const ignoreMwmId = [](Edge const & e1, Edge const & e2)
  {
    if (e1.GetFeatureId().m_index != e2.GetFeatureId().m_index)
      return e1.GetFeatureId().m_index < e2.GetFeatureId().m_index;
    if (e1.IsForward() != e2.IsForward())
      return (e1.IsForward() == false);
    if (e1.GetSegId() != e2.GetSegId())
      return e1.GetSegId() < e2.GetSegId();
    if (!(e1.GetStartJunction() == e2.GetStartJunction()))
      return e1.GetStartJunction() < e2.GetStartJunction();
    if (!(e1.GetEndJunction() == e2.GetEndJunction()))
      return e1.GetEndJunction() < e2.GetEndJunction();
    return false;
  };

  sort(edges1.begin(), edges1.end(), ignoreMwmId);
  sort(edges2.begin(), edges2.end(), ignoreMwmId);
}

bool AreEdgesEqual(IRoadGraph::TEdgeVector const & edgesIndex,
                   IRoadGraph::TEdgeVector const & edgesGeometry)
{
  if (edgesIndex.size() != edgesGeometry.size())
  {
    LOG(LWARNING, ("Wrong size", edgesIndex.size(), edgesGeometry.size()));
    return false;
  }

  for (size_t j = 0; j < edgesIndex.size(); ++j)
  {
    Edge const & fromIndex = edgesIndex[j];
    Edge const & fromGeom = edgesGeometry[j];
    if (!m2::AlmostEqualAbs(fromIndex.GetStartJunction().GetPoint(),
                            fromGeom.GetStartJunction().GetPoint(), kEpsilon))
    {
      LOG(LWARNING, ("Wrong start junction", fromIndex.GetStartJunction().GetPoint(),
                     fromGeom.GetStartJunction().GetPoint()));
      return false;
    }

    if (!m2::AlmostEqualAbs(fromIndex.GetEndJunction().GetPoint(),
                            fromGeom.GetEndJunction().GetPoint(), kEpsilon))
    {
      LOG(LWARNING, ("Wrong end junction", fromIndex.GetEndJunction().GetPoint(),
                     fromGeom.GetEndJunction().GetPoint()));
      return false;
    }

    if (fromIndex.IsForward() != fromGeom.IsForward())
    {
      LOG(LWARNING, ("Wrong forward field", fromIndex.IsForward(), fromGeom.IsForward()));
      return false;
    }

    if (fromIndex.GetFeatureId().m_index != fromGeom.GetFeatureId().m_index)
    {
      LOG(LWARNING, ("Wrong feature id", fromIndex.GetFeatureId().m_index, fromGeom.GetFeatureId().m_index));
      return false;
    }

    if (fromIndex.GetSegId() != fromGeom.GetSegId())
    {
      LOG(LWARNING, ("Wrong seg id", fromIndex.GetSegId(), fromGeom.GetSegId()));
      return false;
    }
  }
  return true;
}
}  // namespace

namespace generator
{
namespace tests_support
{
void TestEdgeIndex(string const & dir, string const & countryName)
{
  LOG(LINFO, ("TestEdgeIndex(", dir, ", ", countryName, ")"));

  Index index;
  LocalCountryFile const localCountryFile(dir, CountryFile(countryName), 1 /* version */);
  auto const regResult = index.RegisterMap(localCountryFile);
  TEST_EQUAL(regResult.second, MwmSet::RegResult::Success, ());

  MwmSet::MwmHandle const mwmHandle = index.GetMwmHandleById(regResult.first);
  TEST(mwmHandle.IsAlive(), ());

  feature::EdgeIndexLoader const loader(*mwmHandle.GetValue<MwmValue>(), mwmHandle.GetId());
  TEST(loader.HasEdgeIndex(), ());

  FeaturesRoadGraph const featureRoadGraph(index, IRoadGraph::Mode::IgnoreOnewayTag,
                                           make_unique<PedestrianModelFactory>());

  size_t wrongOutgoingEdges = 0;
  size_t wrongIngoingEdges = 0;
  size_t allPointsCount = 0;
  auto const processor = [&](FeatureType const & f)
  {
    uint32_t const id = f.GetID().m_index;

    if (!featureRoadGraph.IsRoad(f))
      return;

    if (id % 1000 == 0)
      LOG(LINFO, ("Mwm consistency test. Current id =", id));

    f.ParseGeometry(FeatureType::BEST_GEOMETRY);
    size_t const pointsCount = f.GetPointsCount();

    for (size_t i = 0; i < pointsCount; ++i)
    {
      allPointsCount += 1;
      Junction const junction(f.GetPoint(i), kDefaultAltitudeMeters);
      // |junctionFromLoader| is make exactly the same way as junctions from
      // EdgeIndexLoader are made.
      Junction const junctionFromLoader = RoundJunction(junction);
      // Outgoing edges.
      {
        // Edges from edge index section.
        IRoadGraph::TEdgeVector edgesIndex;
        loader.GetOutgoingEdges(junctionFromLoader, edgesIndex);

        // Edges from geometry.
        IRoadGraph::TEdgeVector edgesGeometry;
        featureRoadGraph.GetOutgoingEdges(junction, edgesGeometry);

        // Comparing outgoing edges for edge index section and from geometry.
        SortEdges(edgesIndex, edgesGeometry);

        if (!AreEdgesEqual(edgesIndex, edgesGeometry))
        {
          wrongOutgoingEdges += 1;
          LOG(LWARNING, (wrongOutgoingEdges, "wrong outgoing edges of", allPointsCount, ":",
                         MercatorBounds::ToLatLon(junction.GetPoint())));
        }
      }

      // Ingoing edges.
      {
        // Edges from edge index section.
        IRoadGraph::TEdgeVector edgesIndex;
        loader.GetIngoingEdges(junctionFromLoader, edgesIndex);

        // Edges from geometry.
        IRoadGraph::TEdgeVector edgesGeometry;
        featureRoadGraph.GetIngoingEdges(junction, edgesGeometry);

        // Comparing ingoing edges for edge index section and from geometry.
        SortEdges(edgesIndex, edgesGeometry);

        if (!AreEdgesEqual(edgesIndex, edgesGeometry))
        {
          wrongIngoingEdges += 1;
          LOG(LWARNING, (wrongIngoingEdges, "wrong ingoing edges of", allPointsCount, ":",
                         MercatorBounds::ToLatLon(junction.GetPoint())));
        }
      }
    }
  };

  index.ForEachInScale(processor, featureRoadGraph.GetStreetReadScale());
}
}  // namespace tests_support
}  // namespace generator
