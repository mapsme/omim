#pragma once

#include "search/house_to_street_table.hpp"

#include "indexer/feature_decl.hpp"

#include "storage/storage_defines.hpp"

#include "coding/string_utf8_multilang.hpp"

#include "base/string_utils.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/optional.hpp>

class FeatureType;
class DataSource;

namespace storage
{
class CountryInfoGetter;
}  // namespace storage

namespace search
{
class MwmContext;
class CityFinder;
class RegionInfoGetter;

class ReverseGeocoder
{
  DataSource const & m_dataSource;

  struct Object
  {
    FeatureID m_id;
    double m_distanceMeters;
    std::string m_name;

    Object() : m_distanceMeters(-1.0) {}
    Object(FeatureID const & id, double dist, std::string const & name)
      : m_id(id), m_distanceMeters(dist), m_name(name)
    {
    }

    inline bool IsValid() const { return m_id.IsValid(); }
  };

  friend std::string DebugPrint(Object const & obj);

public:
  /// All "Nearby" functions work in this lookup radius.
  static int constexpr kLookupRadiusM = 500;

  explicit ReverseGeocoder(DataSource const & dataSource);

  struct Street : public Object
  {
    StringUtf8Multilang m_multilangName;

    Street() = default;
    Street(FeatureID const & id, double dist, std::string const & name,
           StringUtf8Multilang const & multilangName)
      : Object(id, dist, name), m_multilangName(multilangName)
    {
    }
  };

  struct Building : public Object
  {
    m2::PointD m_center;

    // To investigate possible errors.
    // There are no houses in (0, 0) coordinates.
    Building() : m_center(0, 0) {}

    Building(FeatureID const & id, double dist, std::string const & number,
             m2::PointD const & center)
      : Object(id, dist, number), m_center(center)
    {
    }
  };

  struct Address
  {
    Building m_building;
    Street m_street;

    std::string const & GetHouseNumber() const { return m_building.m_name; }
    std::string const & GetStreetName() const { return m_street.m_name; }
    double GetDistance() const { return m_building.m_distanceMeters; }
    bool IsValid() const { return m_building.IsValid() && m_street.IsValid(); }

    // 7 vulica Frunze
    std::string FormatAddress() const;
  };

  struct RegionAddress
  {
    // CountryId is set in case when FeatureID is not found or belongs to World.wmw
    // and we can't get country name from it.
    storage::CountryId m_countryId;
    FeatureID m_featureId;

    bool IsValid() const;
    std::string GetCountryName() const;
    bool operator==(RegionAddress const & rhs) const;
    bool operator!=(RegionAddress const & rhs) const;
    bool operator<(RegionAddress const & rhs) const;
  };

  friend std::string DebugPrint(Address const & addr);

  /// Returns a feature id of street from |streets| whose name best matches |keyName|
  /// or empty value if the match was not found.
  static boost::optional<uint32_t> GetMatchedStreetIndex(std::string const & keyName,
                                                         std::vector<Street> const & streets);

  /// @return Sorted by distance streets vector for the specified MwmId.
  /// Parameter |includeSquaresAndSuburbs| needed for backward compatibility:
  /// existing mwms operate on streets without squares and suburbs.
  static void GetNearbyStreets(search::MwmContext & context, m2::PointD const & center,
                               bool includeSquaresAndSuburbs, std::vector<Street> & streets);
  void GetNearbyStreets(MwmSet::MwmId const & id, m2::PointD const & center,
                        std::vector<Street> & streets) const;
  void GetNearbyStreets(FeatureType & ft, std::vector<Street> & streets) const;

  /// @return feature street name.
  /// Returns empty string when there is no street the feature belongs to.
  std::string GetFeatureStreetName(FeatureType & ft) const;
  /// Same with GetFeatureStreetName but gets street from mwm only (not editor).
  std::string GetOriginalFeatureStreetName(FeatureID const & fid) const;
  /// For |houseId| with street information sets |streetId| to FeatureID of street corresponding to
  /// |houseId| and returns true. Returs false otherwise.
  bool GetStreetByHouse(FeatureType & house, FeatureID & streetId) const;

  /// @return The nearest exact address where building has house number and valid street match.
  void GetNearbyAddress(m2::PointD const & center, Address & addr) const;
  /// @return The nearest exact address where building is at most |maxDistanceM| far from |center|,
  /// has house number and valid street match.
  void GetNearbyAddress(m2::PointD const & center, double maxDistanceM, Address & addr) const;
  /// @param addr (out) the exact address of a feature.
  /// @returns false if  can't extruct address or ft have no house number.
  bool GetExactAddress(FeatureType & ft, Address & addr) const;
  bool GetExactAddress(FeatureID const & fid, Address & addr) const;

  /// Returns the nearest region address where mwm or exact city is known.
  static RegionAddress GetNearbyRegionAddress(m2::PointD const & center,
                                              storage::CountryInfoGetter const & infoGetter,
                                              CityFinder & cityFinder);
  std::string GetLocalizedRegionAddress(RegionAddress const & addr,
                                        RegionInfoGetter const & nameGetter) const;

private:
  /// Helper class to incapsulate house 2 street table reloading.
  class HouseTable
  {
  public:
    explicit HouseTable(DataSource const & dataSource) : m_dataSource(dataSource) {}
    bool Get(FeatureID const & fid, HouseToStreetTable::StreetIdType & type,
             uint32_t & streetIndex);

  private:
    DataSource const & m_dataSource;
    std::unique_ptr<search::HouseToStreetTable> m_table;
    MwmSet::MwmHandle m_handle;
  };

  /// Old data compatible method to retrieve nearby streets.
  void GetNearbyStreetsWaysOnly(MwmSet::MwmId const & id, m2::PointD const & center,
                                std::vector<Street> & streets) const;

  /// Ignores changes from editor if |ignoreEdits| is true.
  bool GetNearbyAddress(HouseTable & table, Building const & bld, bool ignoreEdits,
                        Address & addr) const;

  /// @return Sorted by distance houses vector with valid house number.
  void GetNearbyBuildings(m2::PointD const & center, double maxDistanceM,
                          std::vector<Building> & buildings) const;

  static Building FromFeature(FeatureType & ft, double distMeters);
};

} // namespace search
