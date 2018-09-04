#include "testing/testing.hpp"

#include "routing/routing_integration_tests/routing_test_tools.hpp"

#include "routing/route.hpp"
#include "routing/routing_callbacks.hpp"
#include "routing/routing_helpers.hpp"
#include "routing/routing_session.hpp"
#include "routing/routing_tests/tools.hpp"

#include "platform/location.hpp"
#include "platform/measurement_utils.hpp"

#include <algorithm>
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
  info.m_speedMpS = speed;
  return info;
}

void ChangePosition(RoutingSession & routingSession, ms::LatLon const & coords, double speedKmPH = -1)
{
  static double constexpr kCoordEpsilon = 1e-6;

  routingSession.OnLocationPositionChanged(MoveTo({coords.lat, coords.lon}, KMPH2MPS(speedKmPH)));
  routingSession.OnLocationPositionChanged(MoveTo({coords.lat + kCoordEpsilon,
                                                   coords.lon + kCoordEpsilon}, KMPH2MPS(speedKmPH)));
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
  routingSession.AssignRouteForTesting(std::make_shared<Route>(route), result);
  routingSession.SetTurnNotificationsUnits(measurement_utils::Units::Metric);
  string const engShortJson = R"(
    {
      "unknown_camera": ")" + kCameraOnTheWay + R"("
    }
  )";
  routingSession.SetLocaleWithJsonForTesting(engShortJson, "en");
}

bool IsExistsInNotification(vector<string> const & notifications, string const & value)
{
  auto const it = std::find_if(notifications.begin(), notifications.end(), [&value](auto const & item) {
    return item == value;
  });

  return it != notifications.cend();
}

// These both tests, check notification when drived placed in dangerous zone
// of speed camera.
UNIT_TEST(SpeedCameraNotification_1)
{
  RoutingSession routingSession;
  InitRoutingSession({55.6793107, 37.5326805} /* from */,
                     {55.6876463, 37.5450897} /* to   */,
                     routingSession);

  {
    ChangePosition(routingSession, {55.6812619, 37.5355195}, 100 /* speedKmPH */);
    vector<string> notifications;
    routingSession.GenerateNotifications(notifications);

    TEST(IsExistsInNotification(notifications, kCameraOnTheWay), ());
  }
}

UNIT_TEST(SpeedCameraNotification_2)
{
  RoutingSession routingSession;
  InitRoutingSession({55.7407058, 37.6168184} /* from */,
                     {55.7488593, 37.6103699} /* to   */,
                     routingSession);

  {
    ChangePosition(routingSession, {55.7450529, 37.6138481}, 100 /* speedKmPH */);
    vector<string> notifications;
    routingSession.GenerateNotifications(notifications);

    TEST(IsExistsInNotification(notifications, kCameraOnTheWay), ());
  }
}

// This test about notification before dangerous zone of speed camera.
// It is case in which driver should slow down the speed.
UNIT_TEST(SpeedCameraNotification_3)
{
  RoutingSession routingSession;
  InitRoutingSession({55.7680121, 37.5936353} /* from */,
                     {55.7594795, 37.5848420} /* to   */,
                     routingSession);

  {
    ChangePosition(routingSession, {55.7676667, 37.592604}, 100 /* speedKmPH */);
    vector<string> notifications;
    routingSession.GenerateNotifications(notifications);

    TEST(!IsExistsInNotification(notifications, kCameraOnTheWay), ());
  }

  {
    ChangePosition(routingSession, {55.7658967, 37.5899915}, 100 /* speedKmPH */);
    vector<string> notifications;
    routingSession.GenerateNotifications(notifications);

    TEST(IsExistsInNotification(notifications, kCameraOnTheWay), ());
  }
}

// Test about camera which is not part of way in OSM.
// This link (camera and way) was found by geometry index.
UNIT_TEST(SpeedCameraNotification_4)
{
  RoutingSession routingSession;
  InitRoutingSession({55.6560107, 37.5382251} /* from */,
                     {55.6576076, 37.5231288} /* to   */,
                     routingSession);

  {
    ChangePosition(routingSession, {55.6564793, 37.5364332}, 100 /* speedKmPH */);
    vector<string> notifications;
    routingSession.GenerateNotifications(notifications);

    TEST(!IsExistsInNotification(notifications, kCameraOnTheWay), ());
  }
  {
    ChangePosition(routingSession, {55.6567174, 37.5323678}, 100 /* speedKmPH */);
    vector<string> notifications;
    routingSession.GenerateNotifications(notifications);

    TEST(IsExistsInNotification(notifications, kCameraOnTheWay), ());
  }
}
}  // namespace
