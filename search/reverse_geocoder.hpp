#pragma once

#include "search/house_to_street_table.hpp"

#include "indexer/feature_decl.hpp"

#include "base/string_utils.hpp"

#include "std/string.hpp"
#include "std/utility.hpp"
#include "std/vector.hpp"


class FeatureType;
class Index;

namespace search
{

class ReverseGeocoder
{
  Index const & m_index;

  struct Object
  {
    FeatureID m_id;
    double m_distanceMeters;
    string m_name;

    Object() : m_distanceMeters(-1.0) {}
    Object(FeatureID const & id, double dist, string const & name)
      : m_id(id), m_distanceMeters(dist), m_name(name)
    {
    }

    inline bool IsValid() const { return m_id.IsValid(); }
  };

  friend string DebugPrint(Object const & obj);

public:
  /// All "Nearby" functions work in this lookup radius.
  static int constexpr kLookupRadiusM = 500;

  explicit ReverseGeocoder(Index const & index);

  using Street = Object;

  struct Building : public Object
  {
    m2::PointD m_center;

    // To investigate possible errors.
    // There are no houses in (0, 0) coordinates.
    Building() : m_center(0, 0) {}

    Building(FeatureID const & id, double dist, string const & number, m2::PointD const & center)
      : Object(id, dist, number), m_center(center)
    {
    }
  };

  static size_t GetMatchedStreetIndex(strings::UniString const & keyName,
                                      vector<Street> const & streets);

  struct Address
  {
    Building m_building;
    Street m_street;

    string const & GetHouseNumber() const { return m_building.m_name; }
    string const & GetStreetName() const { return m_street.m_name; }
    double GetDistance() const { return m_building.m_distanceMeters; }
  };

  friend string DebugPrint(Address const & addr);

  /// @return Sorted by distance streets vector for the specified MwmId.
  //@{
  void GetNearbyStreets(MwmSet::MwmId const & id, m2::PointD const & center,
                        vector<Street> & streets) const;
  void GetNearbyStreets(FeatureType & ft, vector<Street> & streets) const;
  //@}

  /// @returns [a lot of] nearby feature's streets and an index of a feature's street.
  /// Returns a value greater than vector size when there are no Street the feature belongs to.
  /// @note returned vector can contain duplicated street segments.
  pair<vector<Street>, uint32_t> GetNearbyFeatureStreets(FeatureType & ft) const;

  /// @return The nearest exact address where building has house number and valid street match.
  void GetNearbyAddress(m2::PointD const & center, Address & addr) const;
  /// @param addr (out) the exact address of a feature.
  /// @returns false if  can't extruct address or ft have no house number.
  bool GetExactAddress(FeatureType const & ft, Address & addr) const;

private:

  /// Helper class to incapsulate house 2 street table reloading.
  class HouseTable
  {
    Index const & m_index;
    unique_ptr<search::HouseToStreetTable> m_table;
    MwmSet::MwmHandle m_handle;
  public:
    explicit HouseTable(Index const & index) : m_index(index) {}
    bool Get(FeatureID const & fid, uint32_t & streetIndex);
  };

  bool GetNearbyAddress(HouseTable & table, Building const & bld, Address & addr) const;

  /// @return Sorted by distance houses vector with valid house number.
  void GetNearbyBuildings(m2::PointD const & center, vector<Building> & buildings) const;

  static Building FromFeature(FeatureType const & ft, double distMeters);
  static m2::RectD GetLookupRect(m2::PointD const & center, double radiusM);
};

} // namespace search
