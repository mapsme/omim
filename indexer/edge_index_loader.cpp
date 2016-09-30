#include "indexer/edge_index_loader.hpp"

#include "geometry/rect2d.hpp"

#include "coding/file_container.hpp"

#include "base/stl_helpers.hpp"

#include "std/algorithm.hpp"

namespace
{
auto const outgoingEdgesComparator = my::LessBy(&feature::FixEdge::m_startPoint);
}  // namespace

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
      for (PointOutgoingEdges const & pointEdges : featureEdges.m_featureOutgoingEdges)
      {
        for (OutgoingEdge const & edge : pointEdges.m_edges)
        {
          m_outgoingEdges.emplace_back(featureEdges.m_featureId, edge.m_forward, edge.m_segId,
                                       pointEdges.m_pointFrom, edge.m_pointTo);
        }
      }
      prevFeatureId = featureEdges.m_featureId;
    }
    sort(m_outgoingEdges.begin(), m_outgoingEdges.end(), outgoingEdgesComparator);

    // Calculating ingoing edges.
    for (size_t i = 0; i < m_outgoingEdges.size(); ++i)
    {
      auto const & key = m_outgoingEdges[i].m_endPoint;
      auto const it = m_ingoingEdgeIndices.find(key);
      if (it == m_ingoingEdgeIndices.cend())
        m_ingoingEdgeIndices.insert(make_pair(key, vector<size_t>({i})));
      else
        it->second.push_back(i);
    }
  }
  catch (Reader::OpenException const & e)
  {
    m_outgoingEdges.clear();
    m_ingoingEdgeIndices.clear();
    LOG(LERROR, ("File", m_countryFileName, "Error while reading", EDGE_INDEX_FILE_TAG, "section.", e.Msg()));
  }
}

bool EdgeIndexLoader::GetOutgoingEdges(routing::Junction const & junction,
                                       routing::IRoadGraph::TEdgeVector & edges) const
{
  m2::PointI const junctionFix(junction.GetPoint() * kFixPointFactor);
  auto const range = equal_range(m_outgoingEdges.cbegin(), m_outgoingEdges.cend(),
                                 FixEdge(junctionFix), outgoingEdgesComparator);
  if (range.first == range.second || range.first == m_outgoingEdges.cend())
  {
    LOG(LERROR, ("m_outgoingEdges doesn't contain junction:", junction));
    return false;
  }

  // @TODO It's necessary add a correct altitude to |junctionFrom| here and below for an end junction.
  routing::Junction const junctionFrom = routing::PointIToJunction(junctionFix);
  for (auto it = range.first; it != range.second; ++it)
  {
    edges.emplace_back(FeatureID(m_mwmId, it->m_featureId), it->m_forward, it->m_segId, junctionFrom,
                       routing::PointIToJunction(it->m_endPoint));
  }

  return true;
}

bool EdgeIndexLoader::GetIngoingEdges(routing::Junction const & junction,
                                      routing::IRoadGraph::TEdgeVector & edges) const
{
  m2::PointI const junctionFix(junction.GetPoint() * kFixPointFactor);
  auto const it = m_ingoingEdgeIndices.find(junctionFix);
  if (it == m_ingoingEdgeIndices.cend())
  {
    LOG(LERROR, ("m_ingoingEdgeIndices doesn't contain junction"));
    return false;
  }

  // @TODO It's necessary add a correct altitude to |junctionTo| here and below for an start junction.
  routing::Junction const junctionTo = routing::PointIToJunction(junctionFix);
  vector<size_t> const & edgeIndices = it->second;
  for (size_t idx : edgeIndices)
  {
    FixEdge const & e = m_outgoingEdges[idx];
    edges.emplace_back(FeatureID(m_mwmId, e.m_featureId), e.m_forward, e.m_segId,
                       routing::PointIToJunction(e.m_startPoint), junctionTo);
  }

  return true;
}
}  // namespace feature
