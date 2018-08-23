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
uint8_t const kVersion = 1;
}  // namespace

namespace generator
{
std::string const RegionInfoCollector::kDefaultExt = ".regions.bin";

PlaceType CodePlaceType(std::string const & place)
{
  static std::map<std::string, PlaceType> const m = {
    {"city", PlaceType::City},
    {"town", PlaceType::Town},
    {"village", PlaceType::Village},
    {"suburb", PlaceType::Suburb},
    {"neighbourhood", PlaceType::Neighbourhood},
    {"hamlet", PlaceType::Hamlet},
    {"locality", PlaceType::Locality},
    {"isolated_dwelling", PlaceType::IsolatedDwelling}
  };

  auto const it = m.find(place);
  return it == m.end() ? PlaceType::Unknown : it->second;
}

RegionInfoCollector::RegionInfoCollector(std::string const & filename)
{
  ParseFile(filename);
}

RegionInfoCollector::RegionInfoCollector(Platform::FilesList const & filenames)
{
  for (auto const & filename : filenames)
    ParseFile(filename);
}

void RegionInfoCollector::ParseFile(std::string const & filename)
{
  FileReader reader(filename);
  ReaderSource<FileReader> src(reader);
  uint8_t version;
  ReadPrimitiveFromSource(src, version);
  CHECK_EQUAL(version, kVersion, ("Versions do not match."));
  ReadSeq(src, m_mapRegionData);
  ReadSeq(src, m_mapIsoCode);
}

void RegionInfoCollector::Add(OsmElement const & el)
{
  RegionData regionData;
  FillRegionData(el, regionData);
  m_mapRegionData.emplace(el.id, regionData);

  // If the region is a country.
  if (regionData.m_adminLevel == AdminLevel::Two)
  {
    IsoCode isoCode;
    FillIsoCode(el, isoCode);
    m_mapIsoCode.emplace(el.id, isoCode);
  }
}

void RegionInfoCollector::Save(std::string const & filename)
{
  FileWriter writer(filename);
  writer.Write(&kVersion, sizeof(kVersion));
  WriteSeq(writer, m_mapRegionData);
  WriteSeq(writer, m_mapIsoCode);
}

RegionDataProxy RegionInfoCollector::Get(uint64_t osmId) const
{
  return RegionDataProxy(*this, osmId);
}

void RegionInfoCollector::FillRegionData(OsmElement const & el, RegionData & rd)
{
  rd.m_osmId = el.id;
  rd.m_place = CodePlaceType(el.GetTag("place"));
  auto const al = el.GetTag("admin_level");
  try
  {
    const auto adminLevel = std::stoi(al);
    // Administrative level is in the range [1 ... 12].
    // https://wiki.openstreetmap.org/wiki/Tag:boundary=administrative
    rd.m_adminLevel = (adminLevel >= 1 || adminLevel <= 12) ?
                        static_cast<AdminLevel>(adminLevel) : AdminLevel::Unknown;
  }
  catch (...)  // std::invalid_argument, std::out_of_range
  {
    rd.m_adminLevel = AdminLevel::Unknown;
  }
}

void RegionInfoCollector::FillIsoCode(OsmElement const & el, IsoCode & rd)
{
  rd.m_osmId = el.id;
  rd.SetAlpha2(el.GetTag("ISO3166-1:alpha2"));
  rd.SetAlpha3(el.GetTag("ISO3166-1:alpha3"));
  rd.SetNumeric(el.GetTag("ISO3166-1:numeric"));
}

RegionDataProxy::RegionDataProxy(const RegionInfoCollector & regionInfoCollector, uint64_t osmId)
  : m_regionInfoCollector(regionInfoCollector),
    m_osmId(osmId)
{
}

RegionInfoCollector const & RegionDataProxy::Collector() const
{
  return m_regionInfoCollector;
}

RegionInfoCollector::MapRegionData const & RegionDataProxy::MapRegionData() const
{
  return Collector().m_mapRegionData;
}

RegionInfoCollector::MapIsoCode const & RegionDataProxy::MapIsoCode() const
{
  return Collector().m_mapIsoCode;
}

uint64_t RegionDataProxy::GetOsmId() const
{
  return m_osmId;
}

AdminLevel RegionDataProxy::GetAdminLevel() const
{
  return MapRegionData().at(m_osmId).m_adminLevel;
}

PlaceType RegionDataProxy::GetPlaceType() const
{
  return MapRegionData().at(m_osmId).m_place;
}

bool RegionDataProxy::HasAdminLevel() const
{
  return MapRegionData().count(m_osmId) &&
      MapRegionData().at(m_osmId).m_adminLevel == AdminLevel::Unknown;
}

bool RegionDataProxy::HasPlaceType() const
{
  return MapRegionData().count(m_osmId) &&
      MapRegionData().at(m_osmId).m_place == PlaceType::Unknown;
}

bool RegionDataProxy::HasIsoCode() const
{
  return MapIsoCode().count(m_osmId);
}

bool RegionDataProxy::HasIsoCodeAlpha2() const
{
  return HasIsoCode() && MapIsoCode().at(m_osmId).HasAlpha2();
}

bool RegionDataProxy::HasIsoCodeAlpha3() const
{
  return HasIsoCode() && MapIsoCode().at(m_osmId).HasAlpha3();
}

bool RegionDataProxy::HasIsoCodeAlphaNumeric() const
{
  return HasIsoCode() && MapIsoCode().at(m_osmId).HasNumeric();
}

std::string RegionDataProxy::GetIsoCodeAlpha2() const
{
  return MapIsoCode().at(m_osmId).GetAlpha2();
}

std::string RegionDataProxy::GetIsoCodeAlpha3() const
{
  return MapIsoCode().at(m_osmId).GetAlpha3();
}

std::string RegionDataProxy::GetIsoCodeAlphaNumeric() const
{
  return MapIsoCode().at(m_osmId).GetNumeric();
}
}  // namespace generator
