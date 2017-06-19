#include "routing/subway_model.hpp"

#include "indexer/classificator.hpp"

namespace routing
{
using namespace std;

routing::VehicleModel::InitListT const g_subwayLimitsDefault = {
    {{"subway_meta", "line"}, kSpeedSubwayLineKMpH},
    {{"subway_meta", "change"}, kSpeedSubwayChangeKMpH},
};

// SubwayModel -------------------------------------------------------------------------------------
SubwayModel::SubwayModel()
  : VehicleModel(classif(), g_subwayLimitsDefault)
{
}

// SubwayModelFactory ------------------------------------------------------------------------------
SubwayModelFactory::SubwayModelFactory() { m_model = make_shared<SubwayModel>(); }

shared_ptr<IVehicleModel> SubwayModelFactory::GetVehicleModel() const { return m_model; }

shared_ptr<IVehicleModel> SubwayModelFactory::GetVehicleModelForCountry(
  string const & /* country */) const
{
  return m_model;
}
}  // namespace routing
