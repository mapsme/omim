#include "search/postcode_points.hpp"

#include "indexer/trie_reader.hpp"

#include "platform/mwm_traits.hpp"

#include "coding/reader_wrapper.hpp"

#include "geometry/mercator.hpp"

#include "base/checked_cast.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <iterator>

#include "defines.hpp"

using namespace std;

namespace search
{
void PostcodePoints::Header::Read(Reader & reader)
{
  NonOwningReaderSource source(reader);
  CHECK_EQUAL(static_cast<uint8_t>(m_version), static_cast<uint8_t>(Version::V0), ());
  m_version = static_cast<Version>(ReadPrimitiveFromSource<uint8_t>(source));
  CHECK_EQUAL(static_cast<uint8_t>(m_version), static_cast<uint8_t>(Version::V0), ());
  m_trieOffset = ReadPrimitiveFromSource<uint32_t>(source);
  m_trieSize = ReadPrimitiveFromSource<uint32_t>(source);
  m_pointsOffset = ReadPrimitiveFromSource<uint32_t>(source);
  m_pointsSize = ReadPrimitiveFromSource<uint32_t>(source);
}

PostcodePoints::PostcodePoints(MwmValue const & value)
{
  auto reader = value.m_cont.GetReader(POSTCODE_POINTS_FILE_TAG);
  m_header.Read(*reader.GetPtr());

  m_trieSubReader = reader.GetPtr()->CreateSubReader(m_header.m_trieOffset, m_header.m_trieSize);
  m_root = trie::ReadTrie<SubReaderWrapper<Reader>, ValueList<Uint64IndexValue>>(
      SubReaderWrapper<Reader>(m_trieSubReader.get()), SingleValueSerializer<Uint64IndexValue>());
  CHECK(m_root, ());

  version::MwmTraits traits(value.GetMwmVersion());
  auto const format = traits.GetCentersTableFormat();
  CHECK_EQUAL(format, version::MwmTraits::CentersTableFormat::EliasFanoMapWithHeader,
              ("Unexpected format."));

  m_pointsSubReader =
      reader.GetPtr()->CreateSubReader(m_header.m_pointsOffset, m_header.m_pointsSize);
  m_points = CentersTable::LoadV1(*m_pointsSubReader);
  CHECK(m_points, ());
}

void PostcodePoints::Get(strings::UniString const & postcode, vector<m2::PointD> & points) const
{
  if (!m_root || !m_points || !m_trieSubReader || !m_pointsSubReader || postcode.empty())
    return;

  auto postcodeIt = postcode.begin();
  auto trieIt = m_root->Clone();

  while (postcodeIt != postcode.end())
  {
    auto it = find_if(trieIt->m_edges.begin(), trieIt->m_edges.end(), [&](auto const & edge) {
      return strings::StartsWith(postcodeIt, postcode.end(), edge.m_label.begin(),
                                 edge.m_label.end());
    });
    if (it == trieIt->m_edges.end())
      return;
    trieIt = trieIt->GoToEdge(distance(trieIt->m_edges.begin(), it));
    postcodeIt += it->m_label.size();
  }

  if (postcodeIt != postcode.end())
    return;

  vector<uint32_t> indexes;
  trieIt->m_values.ForEach([&indexes](auto const & v) {
    indexes.push_back(base::asserted_cast<uint32_t>(v.m_featureId));
  });

  points.resize(indexes.size());
  for (size_t i = 0; i < indexes.size(); ++i)
    CHECK(m_points->Get(indexes[i], points[i]), ());
}
}  // namespace search
