#pragma once

#include "generator/restriction_writer.hpp"
#include "generator/routing_index_generator.hpp"

#include "routing/index_graph_loader.hpp"
#include "routing/restrictions_serialization.hpp"

#include "geometry/latlon.hpp"

#include "base/geo_object_id.hpp"

#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace routing
{
/// This class collects all relations with type restriction and save feature ids of
/// their road feature in text file for using later.
class RestrictionCollector
{
public:
  RestrictionCollector() = default;
  /// \param restrictionPath full path to file with road restrictions in osm id terms.
  /// \param osmIdsToFeatureIdsPath full path to file with mapping from osm ids to feature ids.
  RestrictionCollector(std::string const & targetPath,
                       std::string const & mwmPath,
                       std::string const & country,
                       std::string const & restrictionPath,
                       std::string const & osmIdsToFeatureIdsPath,
                       CountryParentNameGetterFn const & countryParentNameGetterFn);

  bool HasRestrictions() const { return !m_restrictions.empty(); }

  /// \returns true if all restrictions in |m_restrictions| are valid and false otherwise.
  /// \note Complexity of the method is linear in the size of |m_restrictions|.
  bool IsValid() const;

  /// \returns Sorted vector of restrictions.
  std::vector<Restriction> const & GetRestrictions() const { return m_restrictions; }

private:

  static m2::PointD constexpr kNoCoords =
      m2::PointD(std::numeric_limits<double>::max(), std::numeric_limits<double>::max());

  friend void UnitTest_RestrictionTest_ValidCase();
  friend void UnitTest_RestrictionTest_InvalidCase();
  friend void UnitTest_RestrictionTest_ParseRestrictions();

  /// \brief Parses comma separated text file with line in following format:
  /// <type of restrictions>, <osm id 1 of the restriction>, <osm id 2>, and so on
  /// For example:
  /// Only, 335049632, 49356687,
  /// No, 157616940, 157616940,
  /// No, 157616940, 157617107,
  /// \param path path to the text file with restrictions.
  bool ParseRestrictions(std::string const & path);

  /// \brief Adds feature id and corresponding |osmId| to |m_osmIdToFeatureId|.
  void AddFeatureId(uint32_t featureId, base::GeoObjectId osmId);

  /// \brief In case of |coords| not equal kNoCoords, searches point at |prev| with
  /// coords equals to |coords| and checks that the |cur| is outgoes from |prev| at
  /// this point.
  /// In case of |coords| equals to kNoCoords, just checks, that |prev| and |cur| has common
  /// junctions.
  bool FeaturesAreCross(m2::PointD const & coords, uint32_t prev, uint32_t cur);

  Joint::Id GetFirstCommonJoint(uint32_t firstFeatureId, uint32_t secondFeatureId);

  bool FeatureHasPointWithCoords(uint32_t featureId, m2::PointD const & coords);
  /// \brief Adds a restriction (vector of osm id).
  /// \param type is a type of restriction
  /// \param osmIds is osm ids of restriction links
  /// \note This method should be called to add a restriction when feature ids of the restriction
  /// are unknown. The feature ids should be set later with a call of |SetFeatureId(...)| method.
  /// \returns true if restriction is add and false otherwise.
  bool AddRestriction(m2::PointD const & latlon, Restriction::Type type,
                      std::vector<base::GeoObjectId> const & osmIds);

  std::vector<Restriction> m_restrictions;
  std::map<base::GeoObjectId, uint32_t> m_osmIdToFeatureId;

  std::unique_ptr<IndexGraph> m_indexGraph;
};

void FromString(std::string const & str, Restriction::Type & type);
void FromString(std::string const & str, RestrictionWriter::ViaType & type);
void FromString(std::string const & str, double & number);
}  // namespace routing
