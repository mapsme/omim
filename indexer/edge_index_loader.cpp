#include "indexer/edge_index_loader.hpp"

#include "geometry/mercator.hpp"
#include "geometry/rect2d.hpp"

#include "indexer/feature_altitude.hpp"

#include "coding/file_container.hpp"

#include "std/algorithm.hpp"

namespace
{
auto const outgoingEdgesComparator = my::LessBy(&feature::FixEdge::m_startPoint);
}  // namespace

namespace feature
{
EdgeIndexLoader::EdgeIndexLoader(MwmValue const & mwmValue, MwmSet::MwmId const & mwmId)
  : m_mwmInfo(mwmId.GetInfo())
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
    m2::PointI const center = routing::PointDToPointI(limitRect.Center());
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
                                       routing::PointIToPointD(pointEdges.m_pointFrom),
                                       routing::PointIToPointD(edge.m_pointTo));
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

// @TODO EdgeIndexLoader::GetOutgoingEdges() and EdgeIndexLoader::GetIngoingEdges()
// methods fill theirs param |edges| with |kDefaultAltitudeMeters| altitude.
// It's not correct. AltitudeLoader should be used for getting correct altitude.
// But method AltitudeLoader::GetAltitudes(uint32_t featureId, size_t pointCount)
// cannot be used here. It should be used in EdgeIndexLoader::EdgeIndexLoader()
// and type of FixEdge::m_startPoint and FixEdge::m_endPoint should be changed to
// Junction.
bool EdgeIndexLoader::GetOutgoingEdges(routing::Junction const & junction,
                                       routing::IRoadGraph::TEdgeVector & edges) const
{
  auto const range = equal_range(m_outgoingEdges.cbegin(), m_outgoingEdges.cend(),
                                 FixEdge(junction.GetPoint()), outgoingEdgesComparator);
  if (range.first == range.second || range.first == m_outgoingEdges.cend())
  {
    LOG(LERROR, ("m_outgoingEdges doesn't contain junction:",
                 MercatorBounds::ToLatLon(junction.GetPoint()), junction));
    return false;
  }

  for (auto it = range.first; it != range.second; ++it)
  {
    edges.emplace_back(FeatureID(MwmSet::MwmId(m_mwmInfo), it->m_featureId), it->m_forward, it->m_segId, junction,
                       routing::Junction(it->m_endPoint, kDefaultAltitudeMeters));
  }

  return true;
}

bool EdgeIndexLoader::GetIngoingEdges(routing::Junction const & junction,
                                      routing::IRoadGraph::TEdgeVector & edges) const
{
  auto const it = m_ingoingEdgeIndices.find(junction.GetPoint());
  if (it == m_ingoingEdgeIndices.cend())
  {
    LOG(LERROR, ("m_ingoingEdgeIndices doesn't contain junction",
                 MercatorBounds::ToLatLon(junction.GetPoint()), junction));
    return false;
  }

  vector<size_t> const & edgeIndices = it->second;
  for (size_t idx : edgeIndices)
  {
    FixEdge const & e = m_outgoingEdges[idx];
    edges.emplace_back(FeatureID(MwmSet::MwmId(m_mwmInfo), e.m_featureId), e.m_forward, e.m_segId,
                       routing::Junction(e.m_startPoint, kDefaultAltitudeMeters), junction);
  }

  return true;
}
}  // namespace feature
