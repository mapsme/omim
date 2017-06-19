#include "testing/testing.hpp"

#include "routing/subway_model.hpp"

namespace
{
using namespace routing;

UNIT_TEST(SubwayModelSmokeTest)
{
  SubwayModelFactory factory;
  shared_ptr<IVehicleModel> model = factory.GetVehicleModel();

  TEST_EQUAL(model->GetMaxSpeed(), kSpeedSubwayLineKMpH, ());
}
}  // namespace
