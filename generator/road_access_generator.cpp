#include "generator/road_access_generator.hpp"

#include "generator/osm_element.hpp"
#include "generator/osm_id.hpp"
#include "generator/routing_helpers.hpp"

#include "routing/road_access.hpp"
#include "routing/road_access_serialization.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature_data.hpp"

#include "coding/file_container.hpp"
#include "coding/file_writer.hpp"

#include "base/logging.hpp"
#include "base/stl_add.hpp"
#include "base/string_utils.hpp"

#include <initializer_list>

#include "defines.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <ostream>
#include <string>
#include <vector>

using namespace std;

namespace
{
char constexpr kAccessPrivate[] = "access=private";
char constexpr kBarrierGate[] = "barrier=gate";
char constexpr kDelim[] = " \t\r\n";

bool ParseRoadAccess(std::string const & roadAccessPath,
                     map<osm::Id, uint32_t> const & osmIdToFeatureId,
                     routing::RoadAccess & roadAccess)
{
  ifstream stream(roadAccessPath);
  if (!stream)
  {
    LOG(LWARNING, ("Could not open", roadAccessPath));
    return false;
  }

  vector<uint32_t> privateRoads;

  std::string line;
  for (uint32_t lineNo = 1;; ++lineNo)
  {
    if (!std::getline(stream, line))
      break;

    strings::SimpleTokenizer iter(line, kDelim);

    if (!iter)
    {
      LOG(LWARNING, ("Error when parsing road access: empty line", lineNo));
      return false;
    }

    std::string const s = *iter;
    ++iter;

    uint64_t osmId;
    if (!iter || !strings::to_uint64(*iter, osmId))
    {
      LOG(LWARNING, ("Error when parsing road access: bad osm id at line", lineNo));
      return false;
    }

    auto const it = osmIdToFeatureId.find(osm::Id::Way(osmId));
    if (it == osmIdToFeatureId.cend())
    {
      LOG(LWARNING, ("Error when parsing road access: unknown osm id", osmId, "at line", lineNo));
      return false;
    }

    uint32_t const featureId = it->second;
    privateRoads.emplace_back(featureId);
  }

  roadAccess.SetPrivateRoads(move(privateRoads));

  return true;
}
}  // namespace

namespace routing
{
// RoadAccessWriter ------------------------------------------------------------
void RoadAccessWriter::Open(std::string const & filePath)
{
  LOG(LINFO,
      ("Saving information about barriers and road access classes in osm id terms to", filePath));
  m_stream.open(filePath, ofstream::out);

  if (!IsOpened())
    LOG(LINFO, ("Cannot open file", filePath));
}

void RoadAccessWriter::Process(OsmElement const & elem, FeatureParams const & params)
{
  if (!IsOpened())
  {
    LOG(LWARNING, ("Tried to write to a closed barriers writer"));
    return;
  }

  auto const & c = classif();

  my::StringIL const forbiddenRoadTypes[] = {{"hwtag", "private"}};

  for (auto const & f : forbiddenRoadTypes)
  {
    auto const t = c.GetTypeByPath(f);
    if (params.IsTypeExist(t) && elem.type == OsmElement::EntityType::Way)
      m_stream << kAccessPrivate << " " << elem.id << endl;
  }

  auto t = c.GetTypeByPath({"barrier", "gate"});
  if (params.IsTypeExist(t))
    m_stream << kBarrierGate << " " << elem.id << endl;
}

bool RoadAccessWriter::IsOpened() const { return m_stream && m_stream.is_open(); }

// RoadAccessCollector ----------------------------------------------------------
RoadAccessCollector::RoadAccessCollector(std::string const & roadAccessPath,
                                         std::string const & osmIdsToFeatureIdsPath)
{
  map<osm::Id, uint32_t> osmIdToFeatureId;
  if (!ParseOsmIdToFeatureIdMapping(osmIdsToFeatureIdsPath, osmIdToFeatureId))
  {
    LOG(LWARNING, ("An error happened while parsing feature id to osm ids mapping from file:",
                   osmIdsToFeatureIdsPath));
    m_valid = false;
    return;
  }

  RoadAccess roadAccess;
  if (!ParseRoadAccess(roadAccessPath, osmIdToFeatureId, roadAccess))
  {
    LOG(LWARNING, ("An error happened while parsing road access from file:", roadAccessPath));
    m_valid = false;
    return;
  }

  m_valid = true;
  m_roadAccess.Swap(roadAccess);
}

// Functions ------------------------------------------------------------------
void BuildRoadAccessInfo(std::string const & dataFilePath, std::string const & roadAccessPath,
                         std::string const & osmIdsToFeatureIdsPath)
{
  LOG(LINFO, ("Generating road access info for", dataFilePath));

  RoadAccessCollector collector(roadAccessPath, osmIdsToFeatureIdsPath);

  if (!collector.IsValid())
  {
    LOG(LWARNING, ("Unable to parse road access in osm terms"));
    return;
  }

  FilesContainerW cont(dataFilePath, FileWriter::OP_WRITE_EXISTING);
  FileWriter writer = cont.GetWriter(ROAD_ACCESS_FILE_TAG);

  RoadAccessSerializer::Serialize(writer, collector.GetRoadAccess());
}
}  // namespace routing
