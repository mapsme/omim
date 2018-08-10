#include "testing/testing.hpp"

#include "routing/routing_integration_tests/routing_test_tools.hpp"

#include "routing/route.hpp"
#include "routing/routing_callbacks.hpp"
#include "routing/routing_session.hpp"
#include "routing/routing_tests/tools.hpp"

#include "platform/location.hpp"

#include <memory>

using namespace routing;
using namespace routing::turns;
using namespace std;

namespace
{

string const kCameraOnTheWay = "Speed camera on the way";

location::GpsInfo MoveTo(ms::LatLon const & coords, double speed = -1)
{
  location::GpsInfo info;
  info.m_horizontalAccuracy = 0.01;
  info.m_verticalAccuracy = 0.01;
  info.m_latitude = coords.lat;
  info.m_longitude = coords.lon;
  info.m_speed = speed;
  return info;
}

void ChangePosition(RoutingSession & routingSession, ms::LatLon const & coords, double speed = -1)
{
  static double constexpr kCoordEpsilon = 1e-6;

  routingSession.OnLocationPositionChanged(MoveTo({coords.lat, coords.lon}, 100));
  routingSession.OnLocationPositionChanged(MoveTo({coords.lat + kCoordEpsilon, coords.lon + kCoordEpsilon}, 100));
}

void InitRoutingSession(ms::LatLon const & from, ms::LatLon const & to, RoutingSession & routingSession)
{
  TRouteResult const routeResult =
    integration::CalculateRoute(integration::GetVehicleComponents<VehicleType::Car>(),
                                MercatorBounds::FromLatLon(from), {0., 0.},
                                MercatorBounds::FromLatLon(to));

  Route & route = *routeResult.first;
  RouterResultCode const result = routeResult.second;
  TEST_EQUAL(result, RouterResultCode::NoError, ());

  routingSession.Init(nullptr, nullptr);
  routingSession.SetRoutingSettings(routing::GetRoutingSettings(routing::VehicleType::Car));
  routingSession.AssignRouteForTests(std::make_shared<Route>(route), result);
  string const engShortJson = R"(
    {
      "speed_camera": ")" + kCameraOnTheWay + R"("
    }
  )";
  routingSession.ForTestingSetLocaleWithJson(engShortJson, "en");
}

bool IsExistsInNotification(vector<string> const & notifications, string const & value)
{
  for (auto const & item : notifications)
  {
    if (item == value)
      return true;
  }

  return false;
}

UNIT_TEST(SpeedCameraNotification_1)
{
  RoutingSession routingSession;
  InitRoutingSession(/* from = */ {55.6793107, 37.5326805},
                     /* to   = */ {55.6876463, 37.5450897},
                                  routingSession);

  ChangePosition(routingSession, {55.6812619, 37.5355195}, 100);

  vector<string> notifications;
  routingSession.GenerateNotifications(notifications);

  TEST(IsExistsInNotification(notifications, kCameraOnTheWay), ());
}

}  // namespace