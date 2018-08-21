#include "generator/region_info_collector.hpp"

#include "generator/feature_builder.hpp"
#include "generator/osm_element.hpp"

#include "coding/file_writer.hpp"
#include "coding/reader.hpp"
#include "coding/write_to_sink.hpp"

#include "base/logging.hpp"

#include <map>

namespace
{
uint8_t const kVersion = 0;
}  // namespace

namespace generator
{
PlaceType CodePlaceType(const std::string place)
{
  static std::map<std::string, PlaceType> const m = {
    {"city", PlaceType::City}, {"town", PlaceType::Town}, {"village", PlaceType::Village},
    {"suburb", PlaceType::Suburb}, {"neighbourhood", PlaceType::Neighbourhood},
    {"hamlet", PlaceType::Hamlet}, {"locality", PlaceType::Locality},
    {"isolated_dwelling", PlaceType::IsolatedDwelling}
  };

  auto const it = m.find(place);
  return it == m.end() ? PlaceType::None : it->second;
}

const std::string RegionInfoCollector::kDefaultExt = ".regions.bin";

RegionInfoCollector::RegionInfoCollector(const std::string & fileName)
{
  ParseFile(fileName);
}

RegionInfoCollector::RegionInfoCollector(const Platform::FilesList & fileNames)
{
  for (auto const & fileName : fileNames)
    ParseFile(fileName);
}

void RegionInfoCollector::ParseFile(const std::string & fileName)
{
  FileReader reader(fileName);
  ReaderSource<FileReader> src(reader);
  uint8_t version;
  src.Read(&version, sizeof(version));
  ASSERT_EQUAL(version, kVersion, ("Versions do not match."));
  uint32_t size;
  src.Read(&size, sizeof(size));
  RegionData regionData;
  for (uint32_t i = 0; i < size; ++i)
  {
    src.Read(&regionData, sizeof(regionData));
    m_map.emplace(regionData.m_osmId, regionData);
  }
}

void RegionInfoCollector::Add(OsmElement const & el)
{
  RegionData regionData;
  Fill(el, regionData);
  m_map.emplace(el.id, regionData);
}

void RegionInfoCollector::Save(const std::string & fileName)
{
  FileWriter writer(fileName);
  writer.Write(&kVersion, sizeof(kVersion));
  uint32_t const size = static_cast<uint32_t>(m_map.size());
  writer.Write(&size, sizeof(size));
  for (auto const & el : m_map)
    writer.Write(&el.second, sizeof(el.second));
}

RegionData & RegionInfoCollector::Get(uint64_t osmId)
{
  return m_map.at(osmId);
}

const RegionData & RegionInfoCollector::Get(uint64_t osmId) const
{
  return m_map.at(osmId);
}

bool RegionInfoCollector::IsExists(uint64_t osmId) const
{
  return m_map.count(osmId);
}

void RegionInfoCollector::Fill(OsmElement const & el, RegionData & rd)
{
  rd.m_osmId = el.id;
  rd.m_place = CodePlaceType(el.GetTag("place"));
  auto const al = el.GetTag("admin_level");
  try
  {
    const auto adminLevel = std::stoi(al);
    // Administrative level is in the range [1 ... 10].
    rd.m_adminLevel = (adminLevel >= 1 || adminLevel <= 10) ?
                        static_cast<int8_t>(adminLevel) : RegionData::kNoAdminLevel;
  }
  catch(...)  // std::invalid_argument, std::out_of_range
  {
    rd.m_adminLevel = RegionData::kNoAdminLevel;
  }
}
}  // namespace generator
