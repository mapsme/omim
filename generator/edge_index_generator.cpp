#include "generator/edge_index_generator.hpp"

#include "routing/features_road_graph.hpp"
#include "routing/road_graph.hpp"
#include "routing/routing_helpers.hpp"

#include "indexer/feature.hpp"
#include "indexer/feature_edge_index.hpp"
#include "indexer/feature_processor.hpp"
#include "indexer/index.hpp"

#include "geometry/rect2d.hpp"

#include "coding/file_name_utils.hpp"

#include "platform/country_file.hpp"
#include "platform/local_country_file.hpp"

#include "base/logging.hpp"

#include "std/unique_ptr.hpp"
#include "std/vector.hpp"

using namespace feature;
using namespace platform;

namespace
{
class Processor
{
public:
  Processor(string const & dir, string const & country)
    : m_roadGraph(m_index, routing::IRoadGraph::Mode::ObeyOnewayTag,
                  make_unique<routing::PedestrianModelFactory>())
  {
    LocalCountryFile localCountryFile(dir, CountryFile(country), 1 /* version */);
    m_index.RegisterMap(localCountryFile);
    vector<shared_ptr<MwmInfo>> info;
    m_index.GetMwmsInfo(info);
    CHECK_EQUAL(info.size(), 1, ());
    CHECK(info[0], ());
    m_limitRect = info[0]->m_limitRect;
    LOG(LINFO, ("m_limitRect =", m_limitRect));
  }

  void operator()(FeatureType const & f, uint32_t const & id)
  {
    if (id % 1000 == 0)
      LOG(LINFO, ("id =", id, "road features ", m_outgoingEdges.size()));

    if (!routing::IsRoad(feature::TypesHolder(f)))
      return;

    f.ParseGeometry(FeatureType::BEST_GEOMETRY);
    size_t const pointsCount = f.GetPointsCount();
    if (pointsCount == 0)
      return;

    FeatureOutgoingEdges edges(id);
    edges.m_featureOutgoingEdges.resize(pointsCount);
    for (size_t i = 0; i < pointsCount; ++i)
    {
      m2::PointD pointFrom = f.GetPoint(i);
      routing::Junction juctionFrom(pointFrom, kInvalidAltitude);
      edges.m_featureOutgoingEdges[i].m_pointFrom = static_cast<m2::PointI>(pointFrom * kFixPointFactor);
      routing::IRoadGraph::TEdgeVector outgoingEdges;
      m_roadGraph.GetOutgoingEdges(juctionFrom, outgoingEdges);
      for (auto const & edge : outgoingEdges)
      {
        OutgoingEdge processedEdge;
        processedEdge.m_forward = edge.IsForward();
        // @TODO Check if it's a correct way to find an end point.
        processedEdge.m_pointTo = static_cast<m2::PointI>(
            (processedEdge.m_forward ? edge.GetEndJunction().GetPoint()
                                     : edge.GetStartJunction().GetPoint()) * kFixPointFactor);
        edges.m_featureOutgoingEdges[i].m_edges.push_back(move(processedEdge));
      }
    }

    edges.Sort();
    m_outgoingEdges.push_back(move(edges));
  }

  vector<FeatureOutgoingEdges> const & GetOutgoingEdges()
  {
    return m_outgoingEdges;
  }

  m2::RectD const & GetLimitRect()
  {
    return m_limitRect;
  }

private:
  Index m_index;
  routing::FeaturesRoadGraph m_roadGraph;
  vector<FeatureOutgoingEdges> m_outgoingEdges;
  m2::RectD m_limitRect;
};
}  // namespace

namespace routing
{
void BuildOutgoingEdgeIndex(string const & dir, string const & country)
{
  LOG(LINFO, ("dir =", dir, "country", country));
  try
  {
    Processor processor(dir, country);
    string const datFile = my::JoinFoldersToPath(dir, country + DATA_FILE_EXTENSION);
    LOG(LINFO, ("datFile =", datFile));
    feature::ForEachFromDat(datFile, processor);

    FilesContainerW cont(datFile, FileWriter::OP_WRITE_EXISTING);
    FileWriter w = cont.GetWriter(EDGE_INDEX_FILE_TAG);

    vector<FeatureOutgoingEdges> const & outgoingEdges = processor.GetOutgoingEdges();

    EdgeIndexHeader header(outgoingEdges.size());
    header.Serialize(w);

    m2::PointI const center = m2::PointI(processor.GetLimitRect().Center() * kFixPointFactor);
    LOG(LINFO, ("Outgoing edges size =", outgoingEdges.size()));
    uint32_t prevFeatureId = 0;
    for (auto const & featureOutgoingEdges : outgoingEdges)
    {
      featureOutgoingEdges.Serialize(center, prevFeatureId, w);
      prevFeatureId = featureOutgoingEdges.m_featureId;
    }
  }
  catch (RootException const & e)
  {
    LOG(LERROR, ("An exception happened while creating", EDGE_INDEX_FILE_TAG, "section:", e.what()));
  }
}
}  // namespace routing
