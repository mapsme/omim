#pragma once

#include "openlr/openlr_model.hpp"
#include "openlr/road_type_checkers.hpp"

#include "indexer/feature_data.hpp"

#include <map>

class Classificator;
class Index;

namespace feature
{
class TypesHolder;
}

namespace openlr
{
class RoadInfoGetter final
{
public:
  struct RoadInfo
  {
    RoadInfo() = default;

    FunctionalRoadClass m_frc = FunctionalRoadClass::NotAValue;
    FormOfWay m_fow = FormOfWay::NotAValue;
  };

  RoadInfoGetter(Index const & index);

  RoadInfo Get(FeatureID const & fid);

 private:
  FunctionalRoadClass GetFunctionalRoadClass(feature::TypesHolder const & types) const;
  FormOfWay GetFormOfWay(feature::TypesHolder const & types) const;

  Index const & m_index;
  Classificator const & m_c;

  TrunkChecker const m_trunkChecker;
  PrimaryChecker const m_primaryChecker;
  SecondaryChecker const m_secondaryChecker;
  TertiaryChecker const m_tertiaryChecker;
  ResidentialChecker const m_residentialChecker;
  LivingStreetChecker const m_livingStreetChecker;

  std::map<FeatureID, RoadInfo> m_cache;
};
}  // namespace openlr
