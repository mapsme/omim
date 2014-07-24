#include "vehicle_model.hpp"
#include "../indexer/classificator.hpp"
#include "../indexer/feature.hpp"
#include "../indexer/ftypes_matcher.hpp"
#include "../base/macros.hpp"
#include "../std/limits.hpp"

namespace routing
{

static double const kph2mps = 0.2777777777777778;

VehicleModel::SpeedForType const s_carLimits[] = {
  { {"highway", "motorway"},       90 * kph2mps },
  { {"highway", "trunk"},          85 * kph2mps },
  { {"highway", "motorway_link"},  75 * kph2mps },
  { {"highway", "trunk_link"},     70 * kph2mps },
  { {"highway", "primary"},        65 * kph2mps },
  { {"highway", "primary_link"},   60 * kph2mps },
  { {"highway", "secondary"},      55 * kph2mps },
  { {"highway", "secondary_link"}, 50 * kph2mps },
  { {"highway", "tertiary"},       40 * kph2mps },
  { {"highway", "tertiary_link"},  30 * kph2mps },
  { {"highway", "residential"},    25 * kph2mps },
  { {"highway", "pedestrian"},     25 * kph2mps },
  { {"highway", "unclassified"},   25 * kph2mps },
  { {"highway", "service"},        15 * kph2mps },
  { {"highway", "living_street"},  10 * kph2mps },
  { {"highway", "road"},           10 * kph2mps },
  { {"highway", "track"},          5  * kph2mps },
  /// @todo: Add to classificator
  //{ {"highway", "shuttle_train"},  10 * kph2mps },
  //{ {"highway", "ferry"},          5 * kph2mps  },
  //{ {"highway", "default"},        10 * kph2mps },
  /// @todo: check type
  //{ {"highway", "construction"},   40 * kph2mps },
};


CarModel::CarModel()
  : VehicleModel(classif(), vector<VehicleModel::SpeedForType>(s_carLimits, s_carLimits + ARRAY_SIZE(s_carLimits)))
{
}

VehicleModel::VehicleModel(Classificator const & c, vector<SpeedForType> const & speedLimits) : m_maxSpeed(0)
{
  m_onewayType = c.GetTypeByPath(vector<string>(1, "oneway"));

  for (size_t i = 0; i < speedLimits.size(); ++i)
  {
    m_maxSpeed = max(m_maxSpeed, speedLimits[i].m_speed);
    m_types[c.GetTypeByPath(vector<string>(speedLimits[i].m_types, speedLimits[i].m_types + 2))] = speedLimits[i];
  }
}

double VehicleModel::GetSpeed(FeatureType const & f) const
{
  return GetSpeed(feature::TypesHolder(f));
}

double VehicleModel::GetSpeed(feature::TypesHolder const & types) const
{
  double speed = m_maxSpeed * 2;
  for (size_t i = 0; i < types.Size(); ++i)
  {
    uint32_t const type = ftypes::BaseChecker::PrepareFeatureTypeToMatch(types[i]);
    TypesT::const_iterator it = m_types.find(type);
    if (it != m_types.end())
      speed = min(speed, it->second.m_speed);
  }
  if (speed <= m_maxSpeed)
    return speed;

  return 0.0;
}

bool VehicleModel::IsOneWay(FeatureType const & f) const
{
  return IsOneWay(feature::TypesHolder(f));
}

bool VehicleModel::IsOneWay(feature::TypesHolder const & types) const
{
  return types.Has(m_onewayType);
}

}
