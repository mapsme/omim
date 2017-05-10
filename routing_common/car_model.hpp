#pragma once

#include "routing_common/vehicle_model.hpp"

#include <memory>

namespace routing
{

class CarModel : public VehicleModel
{
public:
  CarModel();

  static CarModel const & AllLimitsInstance();
};

class CarModelFactory : public VehicleModelFactory
{
public:
  CarModelFactory();

  // VehicleModelFactory overrides:
  std::shared_ptr<IVehicleModel> GetVehicleModel() const override;
  std::shared_ptr<IVehicleModel> GetVehicleModelForCountry(std::string const & country) const override;

private:
  std::shared_ptr<IVehicleModel> m_model;
};
}  // namespace routing
