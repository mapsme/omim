#include "generator/gen_mwm_info.hpp"

namespace generator
{
// static
uint32_t const OsmID2FeatureID::kHeaderMagic;

OsmID2FeatureID::OsmID2FeatureID() : m_version(Version::V1) {}

OsmID2FeatureID::OsmID2FeatureID(std::string const & filename)
  : OsmID2FeatureID()
{
  CHECK(ReadFromFile(filename), (filename));
}

OsmID2FeatureID::Version OsmID2FeatureID::GetVersion() const { return m_version; }

bool OsmID2FeatureID::ReadFromFile(std::string const & filename)
{
  m_data.clear();
  try
  {
    FileReader reader(filename);
    NonOwningReaderSource src(reader);
    ReadAndCheckHeader(src);
  }
  catch (FileReader::Exception const & e)
  {
    LOG(LERROR, ("Exception while reading osm id to feature id mapping from file", filename,
                 ". Msg:", e.Msg()));
    return false;
  }

  return true;
}

void OsmID2FeatureID::AddIds(indexer::CompositeId const & osmId, uint32_t featureId)
{
  ASSERT(std::find(std::cbegin(m_data), std::cend(m_data), std::make_pair(osmId, featureId)) ==
         std::cend(m_data),
         (osmId));
  m_data.emplace_back(osmId, featureId);
}

std::vector<uint32_t> OsmID2FeatureID::GetFeatureIds(indexer::CompositeId const & id) const
{
  auto const begin = std::lower_bound(std::cbegin(m_data), std::cend(m_data), id,
                                      [](auto const & l, auto const & r) { return l.first < r; });
  auto const end = std::upper_bound(std::cbegin(m_data), std::cend(m_data), id,
                                   [](auto const & l, auto const & r) { return l < r.first; });
  std::vector<uint32_t> ids;
  ids.reserve(static_cast<size_t>(std::distance(begin, end)));
  std::transform(begin, end, std::back_inserter(ids), base::RetrieveSecond());
  return ids;
}

std::vector<uint32_t> OsmID2FeatureID::GetFeatureIds(base::GeoObjectId mainId) const
{
  std::vector<uint32_t> ids;
  auto it = std::lower_bound(std::cbegin(m_data), std::cend(m_data), mainId,
                             [](auto const & l, auto const & r) { return l.first.m_mainId < r; });
  while (it != std::cend(m_data) && it->first.m_mainId == mainId)
    ids.emplace_back((it++)->second);
  return ids;
}
}  // namespace generator

