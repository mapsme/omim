#include "routing_common/car_model.hpp"
#include "routing_common/car_model_coefs.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature.hpp"

#include "base/macros.hpp"

#include <array>
#include <unordered_map>

using namespace std;
using namespace routing;

namespace
{
// See model specifics in different countries here:
//   https://wiki.openstreetmap.org/wiki/OSM_tags_for_routing/Access-Restrictions

// See road types here:
//   https://wiki.openstreetmap.org/wiki/Key:highway

// Maximum speed for former version. Please see VehicleModel::TuneMaxModelSpeed(...) for details.
InOutCitySpeedKMpH constexpr kFormerModelMaxSpeed = {
    {117.8 /* weight */, 104.7 /* eta */} /* in city */,
    {123.4 /* weight */, 117.07 /* eta */} /* out city */};

//  // Names must be the same with country names from countries.txt
std::array<char const *, 41> constexpr kCountries = {"Australia",
                                                     "Austria",
                                                     "Belarus",
                                                     "Belgium",
                                                     "Brazil",
                                                     "Canada",
                                                     "Colombia",
                                                     "Czech Republic",
                                                     "Denmark",
                                                     "Ecuador",
                                                     "Finland",
                                                     "France",
                                                     "Germany",
                                                     "Hungary",
                                                     "Indonesia",
                                                     "Ireland",
                                                     "Italy",
                                                     "Kuwait",
                                                     "Luxembourg",
                                                     "Mexico",
                                                     "Netherlands",
                                                     "New Zealand",
                                                     "Norway",
                                                     "Poland",
                                                     "Portugal",
                                                     "Romania",
                                                     "Russian Federation",
                                                     "Saudi Arabia",
                                                     "Singapore",
                                                     "Slovakia",
                                                     "South Africa",
                                                     "Spain",
                                                     "Sweden",
                                                     "Switzerland",
                                                     "Thailand",
                                                     "Turkey",
                                                     "Ukraine",
                                                     "United Arab Emirates",
                                                     "United Kingdom",
                                                     "United States of America",
                                                     "Venezuela"};

// |kSpeedOffroadKMpH| is a speed which is used for edges that don't lie on road features.
// For example for pure fake edges. In car routing, off road speed for calculation ETA is not used.
// The weight of such edges is considered as 0 seconds. It's especially actual when an airport is
// a start or finish. On the other hand, while route calculation the fake edges are considered
// as quite heavy. The idea behind that is to use the closest edge for the start and the finish
// of the route except for some edge cases.
SpeedKMpH constexpr kSpeedOffroadKMpH = {0.01 /* weight */, kNotUsed /* eta */};

VehicleModel::LimitsInitList const kCarOptionsDefault = {
    // {{roadType, roadType}  passThroughAllowed}
    {{"highway", "motorway"}, true},
    {{"highway", "motorway_link"}, true},
    {{"highway", "trunk"}, true},
    {{"highway", "trunk_link"}, true},
    {{"highway", "primary"}, true},
    {{"highway", "primary_link"}, true},
    {{"highway", "secondary"}, true},
    {{"highway", "secondary_link"}, true},
    {{"highway", "tertiary"}, true},
    {{"highway", "tertiary_link"}, true},
    {{"highway", "residential"}, true},
    {{"highway", "unclassified"}, true},
    {{"highway", "service"}, true},
    {{"highway", "living_street"}, true},
    {{"highway", "road"}, true},
    {{"highway", "track"}, true}
    /// @todo: Add to classificator
    //{ {"highway", "shuttle_train"},  10 },
    //{ {"highway", "ferry"},          5  },
    //{ {"highway", "default"},        10 },
    /// @todo: Check type
    //{ {"highway", "construction"},   40 },
};

VehicleModel::LimitsInitList const kCarOptionsNoPassThroughLivingStreet = {
    {{"highway", "motorway"}, true},
    {{"highway", "motorway_link"}, true},
    {{"highway", "trunk"}, true},
    {{"highway", "trunk_link"}, true},
    {{"highway", "primary"}, true},
    {{"highway", "primary_link"}, true},
    {{"highway", "secondary"}, true},
    {{"highway", "secondary_link"}, true},
    {{"highway", "tertiary"}, true},
    {{"highway", "tertiary_link"}, true},
    {{"highway", "residential"}, true},
    {{"highway", "unclassified"}, true},
    {{"highway", "service"}, true},
    {{"highway", "living_street"}, false},
    {{"highway", "road"}, true},
    {{"highway", "track"}, true}};

VehicleModel::LimitsInitList const kCarOptionsNoPassThroughLivingStreetAndService = {
    {{"highway", "motorway"}, true},
    {{"highway", "motorway_link"}, true},
    {{"highway", "trunk"}, true},
    {{"highway", "trunk_link"}, true},
    {{"highway", "primary"}, true},
    {{"highway", "primary_link"}, true},
    {{"highway", "secondary"}, true},
    {{"highway", "secondary_link"}, true},
    {{"highway", "tertiary"}, true},
    {{"highway", "tertiary_link"}, true},
    {{"highway", "residential"}, true},
    {{"highway", "unclassified"}, true},
    {{"highway", "service"}, false},
    {{"highway", "living_street"}, false},
    {{"highway", "road"}, true},
    {{"highway", "track"}, true}};

VehicleModel::LimitsInitList const kCarOptionsDenmark = {
    // No track
    {{"highway", "motorway"}, true},
    {{"highway", "motorway_link"}, true},
    {{"highway", "trunk"}, true},
    {{"highway", "trunk_link"}, true},
    {{"highway", "primary"}, true},
    {{"highway", "primary_link"}, true},
    {{"highway", "secondary"}, true},
    {{"highway", "secondary_link"}, true},
    {{"highway", "tertiary"}, true},
    {{"highway", "tertiary_link"}, true},
    {{"highway", "residential"}, true},
    {{"highway", "unclassified"}, true},
    {{"highway", "service"}, true},
    {{"highway", "living_street"}, true},
    {{"highway", "road"}, true}};

VehicleModel::LimitsInitList const kCarOptionsGermany = {
    // No pass through track
    {{"highway", "motorway"}, true},
    {{"highway", "motorway_link"}, true},
    {{"highway", "trunk"}, true},
    {{"highway", "trunk_link"}, true},
    {{"highway", "primary"}, true},
    {{"highway", "primary_link"}, true},
    {{"highway", "secondary"}, true},
    {{"highway", "secondary_link"}, true},
    {{"highway", "tertiary"}, true},
    {{"highway", "tertiary_link"}, true},
    {{"highway", "residential"}, true},
    {{"highway", "unclassified"}, true},
    {{"highway", "service"}, true},
    {{"highway", "living_street"}, true},
    {{"highway", "road"}, true},
    {{"highway", "track"}, false}};

vector<VehicleModel::AdditionalRoadTags> const kAdditionalTags = {
    // {{highway tags}, {weightSpeed, etaSpeed}}
    {{"route", "ferry", "motorcar"}, kGlobalHighwayBasedMeanSpeeds.at(HighwayType::RouteFerryMotorcar)},
    {{"route", "ferry", "motor_vehicle"}, kGlobalHighwayBasedMeanSpeeds.at(HighwayType::RouteFerryMotorVehicle)},
    {{"railway", "rail", "motor_vehicle"}, kGlobalHighwayBasedMeanSpeeds.at(HighwayType::RailwayRailMotorVehicle)},
    {{"route", "shuttle_train"}, kGlobalHighwayBasedMeanSpeeds.at(HighwayType::RouteShuttleTrain)},
    {{"route", "ferry"}, kGlobalHighwayBasedMeanSpeeds.at(HighwayType::RouteFerryMotorcar)},
    {{"man_made", "pier"}, kGlobalHighwayBasedMeanSpeeds.at(HighwayType::ManMadePier)}};

VehicleModel::SurfaceInitList const kCarSurface = {
  // {{surfaceType, surfaceType}, {weightFactor, etaFactor}}
  {{"psurface", "paved_good"}, {1.0, 1.0}},
  {{"psurface", "paved_bad"}, {0.5, 0.5}},
  {{"psurface", "unpaved_good"}, {0.5, 0.8}},
  {{"psurface", "unpaved_bad"}, {0.3, 0.3}}
};

std::unordered_map<char const *, VehicleModel::LimitsInitList> const kCarOptionsByCountries = {
    {"Austria", kCarOptionsNoPassThroughLivingStreet},
    {"Belarus", kCarOptionsNoPassThroughLivingStreet},
    {"Denmark", kCarOptionsDenmark},
    {"Germany", kCarOptionsGermany},
    {"Hungary", kCarOptionsNoPassThroughLivingStreet},
    {"Romania", kCarOptionsNoPassThroughLivingStreet},
    {"Russian Federation", kCarOptionsNoPassThroughLivingStreetAndService},
    {"Slovakia", kCarOptionsNoPassThroughLivingStreet},
    {"Ukraine", kCarOptionsNoPassThroughLivingStreetAndService}
};
}  // namespace

namespace routing
{
CarModel::CarModel()
  : VehicleModel(classif(), kCarOptionsDefault, kCarSurface,
                 {kGlobalHighwayBasedMeanSpeeds, kGlobalHighwayBasedFactors})
{
  Init();
}

CarModel::CarModel(VehicleModel::LimitsInitList const & roadLimits, HighwayBasedInfo const & info)
  : VehicleModel(classif(), roadLimits, kCarSurface, info)
{
  Init();
}

SpeedKMpH const & CarModel::GetOffroadSpeed() const { return kSpeedOffroadKMpH; }

void CarModel::Init()
{
  m_noCarType = classif().GetTypeByPath({"hwtag", "nocar"});
  m_yesCarType = classif().GetTypeByPath({"hwtag", "yescar"});

  SetAdditionalRoadTypes(classif(), kAdditionalTags);
  TuneMaxModelSpeed(kFormerModelMaxSpeed);
}

VehicleModelInterface::RoadAvailability CarModel::GetRoadAvailability(feature::TypesHolder const & types) const
{
  if (types.Has(m_yesCarType))
    return RoadAvailability::Available;

  if (types.Has(m_noCarType))
    return RoadAvailability::NotAvailable;

  return RoadAvailability::Unknown;
}

// static
CarModel const & CarModel::AllLimitsInstance()
{
  static CarModel const instance;
  return instance;
}

// static
routing::VehicleModel::LimitsInitList const & CarModel::GetOptions() { return kCarOptionsDefault; }

// static
vector<routing::VehicleModel::AdditionalRoadTags> const & CarModel::GetAdditionalTags()
{
  return kAdditionalTags;
}

// static
VehicleModel::SurfaceInitList const & CarModel::GetSurfaces() { return kCarSurface; }

CarModelFactory::CarModelFactory(CountryParentNameGetterFn const & countryParentNameGetterFn)
  : VehicleModelFactory(countryParentNameGetterFn)
{
  auto const & speeds = kCountryToHighwayBasedMeanSpeeds;
  auto const & factors = kCountryToHighwayBasedFactors;
  m_models[""] = make_shared<CarModel>(
      kCarOptionsDefault,
      HighwayBasedInfo(kGlobalHighwayBasedMeanSpeeds, kGlobalHighwayBasedFactors));
  for (auto const * country : kCountries)
  {
    auto const limitIt = kCarOptionsByCountries.find(country);
    auto const & limit = limitIt == kCarOptionsByCountries.cend() ? kCarOptionsDefault : limitIt->second;
    auto const speedIt = speeds.find(country);
    auto const & speed = speedIt == speeds.cend() ? kGlobalHighwayBasedMeanSpeeds : speedIt->second;
    auto const factorIt = factors.find(country);
    auto const & factor =
        factorIt == factors.cend() ? kGlobalHighwayBasedFactors : factorIt->second;
    m_models[country] = make_shared<CarModel>(
        limit,
        HighwayBasedInfo(speed, kGlobalHighwayBasedMeanSpeeds, factor, kGlobalHighwayBasedFactors));
  }
}
}  // namespace routing
