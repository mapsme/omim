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
#include "base/stl_helpers.hpp"

#include "std/algorithm.hpp"
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
    : m_roadGraph(m_index, routing::IRoadGraph::Mode::IgnoreOnewayTag,
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

  void operator()(FeatureType const & f)
  {
    uint32_t const id = f.GetID().m_index;
    if (m_outgoingEdges.size() != 0 && m_outgoingEdges.size() % 1000 == 0)
      LOG(LINFO, ("id =", id, "road features ", m_outgoingEdges.size()));

    if (!m_roadGraph.IsRoad(f))
      return;

    f.ParseGeometry(FeatureType::BEST_GEOMETRY);
    size_t const pointsCount = f.GetPointsCount();
    if (pointsCount == 0)
      return;

    FeatureOutgoingEdges edges(id);
    edges.m_featureOutgoingEdges.resize(pointsCount);
    // Node 1. In the code below all outgoing edges for all feature points are extracted
    // with method FeaturesRoadGraph::GetOutgoingEdges(...). Then only edges belongs to the
    // feature are saved in |m_outgoingEdges|. The idea behind it is
    // to get rid of saving duplicate edges. The thing is every edge belongs to a feature.
    // Saving only edges belong to a considered feature no egde would be lost and
    // no edge would be saved twice. On the other hand there's a problem in that case.
    // After edges are saved only judging by rounded to PointI coordinates it's possible
    // to find all outgoing edges from a junction. Consequently it's possible that
    // result of FeaturesRoadGraph::GetOutgoingEdges(...) and EdgeIndexLoader::GetOutgoingEdges(...)
    // are different.
    // @TODO It looks like that the result of FeaturesRoadGraph::GetOutgoingEdges(...) and
    // EdgeIndexLoader::GetOutgoingEdges(...) are different in very rare case.
    // It's necessaret to check it.

    // Node 2. Another disadvantage of such way of saving graph is that a wrong result would be
    // saved in case of features go in different levels but with close points. Probably
    // another representation of graph would be better.
    for (size_t i = 0; i < pointsCount; ++i)
    {
      m2::PointD const pointFrom = f.GetPoint(i);
      routing::Junction const juctionFrom(pointFrom, kInvalidAltitude);
      edges.m_featureOutgoingEdges[i].m_pointFrom = routing::PointDToPointI(pointFrom);
      routing::IRoadGraph::TEdgeVector outgoingEdges;
      m_roadGraph.GetOutgoingEdges(juctionFrom, outgoingEdges);
      for (auto const & edge : outgoingEdges)
      {
        if (edge.GetFeatureId().m_index != id)
          continue;
        // |edge| belongs to |f|.
        edges.m_featureOutgoingEdges[i].m_edges.emplace_back(
            routing::PointDToPointI(edge.GetEndJunction().GetPoint()),
            edge.GetSegId(), edge.IsForward());
      }
    }

    // Note. It's necessary to use SortUnique because some features form loops. That means
    // the same feature contains a point more than once. In that case edges outgoing
    // from the point are included more than once. So it's necessary to remove such duplicates.
    edges.SortUnique();
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

  void ForEachFeature()
  {
    m_index.ForEachInScale(*this, m_roadGraph.GetStreetReadScale());
  }

  void Sort()
  {
    sort(m_outgoingEdges.begin(), m_outgoingEdges.end(), my::LessBy(&FeatureOutgoingEdges::m_featureId));
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
    processor.ForEachFeature();
    processor.Sort();

    FilesContainerW cont(datFile, FileWriter::OP_WRITE_EXISTING);
    FileWriter w = cont.GetWriter(EDGE_INDEX_FILE_TAG);

    vector<FeatureOutgoingEdges> const & outgoingEdges = processor.GetOutgoingEdges();

    EdgeIndexHeader header(outgoingEdges.size());
    header.Serialize(w);

    m2::PointI const center = routing::PointDToPointI(processor.GetLimitRect().Center());
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
