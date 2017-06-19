#pragma once

#include "routing_common/vehicle_model.hpp"

#include "std/shared_ptr.hpp"

namespace routing
{
double constexpr kSpeedSubwayLineKMpH = 30.0;
double constexpr kSpeedSubwayChangeKMpH = 3.0;

class SubwayModel : public VehicleModel
{
public:
  SubwayModel();

  static SubwayModel const & AllLimitsInstance();
};

class SubwayModelFactory : public VehicleModelFactory
{
public:
  SubwayModelFactory();

  // VehicleModelFactory overrides:
  shared_ptr<IVehicleModel> GetVehicleModel() const override;
  shared_ptr<IVehicleModel> GetVehicleModelForCountry(string const & country) const override;

private:
  shared_ptr<IVehicleModel> m_model;
};
}  // namespace routing
