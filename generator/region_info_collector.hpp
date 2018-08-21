#pragma once

#include "platform/platform.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

struct OsmElement;
class FeatureParams;
class FileWriter;

namespace generator
{
enum PlaceType: int8_t
{
  None,
  City,
  Town,
  Village,
  Suburb,
  Neighbourhood,
  Hamlet,
  Locality,
  IsolatedDwelling,

  Count
};

PlaceType CodePlaceType(const std::string place);

struct alignas(1) RegionData
{
  static int8_t const kNoAdminLevel = 0;

  uint64_t m_osmId = 0;
  int8_t m_adminLevel = kNoAdminLevel;
  PlaceType m_place = PlaceType::None;
};

// This is a class for working a file with additional information about regions.
class RegionInfoCollector
{
public:
  static const std::string kDefaultExt;  // = ".regions.bin"

  RegionInfoCollector() = default;
  explicit RegionInfoCollector(const std::string & fileName);
  explicit RegionInfoCollector(const Platform::FilesList & fileNames);

  // It is supposed to be called already on the filtered osm objects that represent regions.
  void Add(OsmElement const & el);
  RegionData & Get(uint64_t osmId);
  const RegionData & Get(uint64_t osmId) const;
  bool IsExists(uint64_t osmId) const;
  void Save(const std::string & fileName);

private:
  using Map = std::unordered_map<uint64_t, RegionData>;

  void ParseFile(const std::string & fileName);
  void Fill(OsmElement const & el, RegionData & rd);

  Map m_map;
};
}  // namespace generator
