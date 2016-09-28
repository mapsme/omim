#include "indexer/edge_index_loader.hpp"

#include "geometry/rect2d.hpp"

#include "coding/file_container.hpp"

#include "std/algorithm.hpp"

//namespace
//{
//void PointOutgoingEdges2TEdgeVector(feature::PointOutgoingEdges const & featureOutgoingEdges,
//                                    routing::IRoadGraph::TEdgeVector & edgeVector)
//{

//}
//}

namespace feature
{
EdgeIndexLoader::EdgeIndexLoader(MwmValue const & mwmValue, MwmSet::MwmId const & mwmId)
  : m_mwmId(mwmId)
{
  m_countryFileName = mwmValue.GetCountryFileName();

  if (mwmValue.GetHeader().GetFormat() < version::Format::v8)
    return;
  if (!mwmValue.m_cont.IsExist(EDGE_INDEX_FILE_TAG))
    return;

  try
  {
    FilesContainerR::TReader reader(mwmValue.m_cont.GetReader(EDGE_INDEX_FILE_TAG));
    ReaderSource<FilesContainerR::TReader> src(reader);
    m_header.Deserialize(src);

    // @TODO Checks if mwmValue.GetHeader().GetBounds() == MwmInfo.m_limitRect
    m2::RectD const limitRect = mwmValue.GetHeader().GetBounds();
    LOG(LINFO, ("limitRect =", limitRect));
    m2::PointI const center = m2::PointI(limitRect.Center() * kFixPointFactor);
    uint32_t prevFeatureId = 0;

    // Reading outgoing edges.
    for (uint32_t i = 0; i < m_header.m_featureCount; ++i)
    {
      FeatureOutgoingEdges featureEdges;
      featureEdges.Deserialize(center, prevFeatureId, src);

      vector<PointOutgoingEdges> output(m_outgoingEdges.size() + featureEdges.m_featureOutgoingEdges.size());
      merge(m_outgoingEdges.begin(), m_outgoingEdges.end(),
            featureEdges.m_featureOutgoingEdges.begin(), featureEdges.m_featureOutgoingEdges.end(),
            output.begin(), OutgoingEdgeSortFunc);
      m_outgoingEdges.swap(output);

      prevFeatureId = featureEdges.m_featureId;
    }

    // Calculating ingoing edges.

    // Calculating edge feature id for outgoing edges.

    // Calculating edge feature id for ingoing edges.
  }
  catch (Reader::OpenException const & e)
  {
    m_outgoingEdges.clear();
//    m_ingoingEdges.clear();
    LOG(LERROR, ("File", m_countryFileName, "Error while reading", EDGE_INDEX_FILE_TAG, "section.", e.Msg()));
  }
}

bool EdgeIndexLoader::GetOutgoingEdges(routing::Junction const & junction,
                                       routing::IRoadGraph::TEdgeVector & edges) const
{
  m2::PointI const junctionFix(junction.GetPoint() * kFixPointFactor);
  auto const it = lower_bound(m_outgoingEdges.cbegin(), m_outgoingEdges.cend(),
                              PointOutgoingEdges(junctionFix), OutgoingEdgeSortFunc);
  if (it == m_outgoingEdges.cend() || it->m_pointFrom != junctionFix)
  {
    LOG(LERROR, ("m_outgoingEdges doesn't contain junction"));
    return false;
  }

  PointOutgoingEdges const & junctionOutgoingEdges = *it;
  routing::Junction const junctionFrom = routing::PointIToJunction(junctionOutgoingEdges.m_pointFrom);
  edges.clear();
  for (uint32_t i = 0; i < junctionOutgoingEdges.m_edges.size(); ++i)
  {
    OutgoingEdge const & pointTo = junctionOutgoingEdges.m_edges[i];
    edges.emplace_back(FeatureID(m_mwmId, pointTo.m_featureId), pointTo.m_forward, pointTo.m_segId,
                       junctionFrom, routing::PointIToJunction(pointTo.m_pointTo));
  }
  return true;
}

}  // namespace feature
