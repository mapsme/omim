#pragma once

#include "platform/platform.hpp"

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>

struct OsmElement;
class FeatureParams;
class FileWriter;

namespace generator
{
// https://wiki.openstreetmap.org/wiki/Tag:boundary=administrative
enum class AdminLevel : uint8_t
{
  Unknown = 0,
  One = 1,
  Two = 2,
  Three = 3,
  Four = 4,
  Five = 5,
  Six = 6,
  Seven = 7,
  Eight = 8,
  Nine = 9,
  Ten = 10,
  Eleven = 11,
  Twelve = 12,

  Count
};

// https://wiki.openstreetmap.org/wiki/Key:place
// Warning: values are important, be careful they are used in Region::GetRank() in regions_kv.cpp
enum class PlaceType: uint8_t
{
  Unknown,
  City = 9,
  Town = 10,
  Village = 11,
  Suburb = 12,
  Neighbourhood = 13,
  Hamlet = 14,
  Locality = 15,
  IsolatedDwelling = 16,

  Count
};

PlaceType CodePlaceType(std::string const & place);

class RegionDataProxy;

// This is a class for working a file with additional information about regions.
class RegionInfoCollector
{
public:
  static std::string const kDefaultExt;

  RegionInfoCollector() = default;
  explicit RegionInfoCollector(std::string const & filename);
  explicit RegionInfoCollector(Platform::FilesList const & filenames);

  // It is supposed to be called already on the filtered osm objects that represent regions.
  void Add(OsmElement const & el);
  RegionDataProxy Get(uint64_t osmId) const;
  void Save(std::string const & filename);

private:
  friend class RegionDataProxy;

  struct alignas(1) IsoCode
  {
    bool HasAlpha2() const { return m_alpha2[0] != '\0'; }
    bool HasAlpha3() const { return m_alpha3[0] != '\0'; }
    bool HasNumeric() const { return m_numeric[0] != '\0'; }

    void SetAlpha2(std::string const & alpha2) { std::strcpy(m_alpha2, alpha2.data()); }
    void SetAlpha3(std::string const & alpha3) { std::strcpy(m_alpha3, alpha3.data()); }
    void SetNumeric(std::string const & numeric) { std::strcpy(m_numeric, numeric.data()); }

    std::string GetAlpha2() const { return m_alpha2; }
    std::string GetAlpha3() const { return m_alpha3; }
    std::string GetNumeric() const { return m_numeric; }

    uint64_t m_osmId = 0;
    char m_alpha2[3] = {};
    char m_alpha3[4] = {};
    char m_numeric[4] = {};
  };

  struct alignas(1) RegionData
  {
    uint64_t m_osmId = 0;
    AdminLevel m_adminLevel = AdminLevel::Unknown;
    PlaceType m_place = PlaceType::Unknown;
  };

  using MapRegionData = std::unordered_map<uint64_t, RegionData>;
  using MapIsoCode = std::unordered_map<uint64_t, IsoCode>;

  template<typename Reader, typename Map>
  void ReadSeq(Reader & src, Map & seq)
  {
    uint32_t size = 0;
    ReadPrimitiveFromSource(src, size);
    typename Map::mapped_type data;
    for (uint32_t i = 0; i < size; ++i)
    {
      ReadPrimitiveFromSource(src, data);
      seq.emplace(data.m_osmId, data);
    }
  }

  template<typename Writer, typename Map>
  void WriteSeq(Writer & writer, Map & seq)
  {
    uint32_t const sizeRegionData = static_cast<uint32_t>(seq.size());
    writer.Write(&sizeRegionData, sizeof(sizeRegionData));
    for (auto const & el : seq)
      writer.Write(&el.second, sizeof(el.second));
  }

  void ParseFile(std::string const & filename);
  void FillRegionData(OsmElement const & el, RegionData & rd);
  void FillIsoCode(OsmElement const & el, IsoCode & rd);

  MapRegionData m_mapRegionData;
  MapIsoCode m_mapIsoCode;
};

class RegionDataProxy
{
public:
  RegionDataProxy(RegionInfoCollector const & regionInfoCollector, uint64_t osmId);

  uint64_t GetOsmId() const;
  AdminLevel GetAdminLevel() const;
  PlaceType GetPlaceType() const;

  bool HasAdminLevel() const;
  bool HasPlaceType() const;

  bool HasIsoCode() const;
  bool HasIsoCodeAlpha2() const;
  bool HasIsoCodeAlpha3() const;
  bool HasIsoCodeAlphaNumeric() const;

  std::string GetIsoCodeAlpha2() const;
  std::string GetIsoCodeAlpha3() const;
  std::string GetIsoCodeAlphaNumeric() const;

private:
  RegionInfoCollector const & Collector() const;
  RegionInfoCollector::MapRegionData const & MapRegionData() const;
  RegionInfoCollector::MapIsoCode const & MapIsoCode() const;

  std::reference_wrapper<const RegionInfoCollector> m_regionInfoCollector;
  uint64_t m_osmId;
};
}  // namespace generator
