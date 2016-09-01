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
EdgeIndexLoader::EdgeIndexLoader(MwmValue const & mwmValue)
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

  }
  catch (Reader::OpenException const & e)
  {
    m_outgoingEdges.clear();
//    m_ingoingEdges.clear();
    LOG(LERROR, ("File", m_countryFileName, "Error while reading", EDGE_INDEX_FILE_TAG, "section.", e.Msg()));
  }
}
}  // namespace feature
