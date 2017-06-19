#pragma once

#include "routing_common/vehicle_model.hpp"

#include <memory>
#include <string>

namespace routing
{
double constexpr kSpeedSubwayLineKMpH = 38.0;
double constexpr kSpeedSubwayChangeKMpH = 3.0;

class SubwayModel : public VehicleModel
{
public:
  SubwayModel();
};

class SubwayModelFactory : public VehicleModelFactory
{
public:
  SubwayModelFactory();

  // VehicleModelFactory overrides:
  std::shared_ptr<IVehicleModel> GetVehicleModel() const override;
  std::shared_ptr<IVehicleModel> GetVehicleModelForCountry(std::string const & country) const override;

private:
  std::shared_ptr<IVehicleModel> m_model;
};
}  // namespace routing
