#include "testing/testing.hpp"

#include "routing/routing_integration_tests/routing_test_tools.hpp"

#include "routing/route.hpp"


using namespace routing;
using namespace routing::turns;

UNIT_TEST(RussiaMoscowNagatinoUturnTurnTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(),
      MercatorBounds::FromLatLon(55.67251, 37.63604), {-0.004, -0.01},
      MercatorBounds::FromLatLon(55.67293, 37.63507));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;
  TEST_EQUAL(result, IRouter::NoError, ());

  integration::TestTurnCount(route, 3);

  integration::GetNthTurn(route, 0)
      .TestValid()
      .TestDirection(TurnDirection::TurnRight);
  integration::GetNthTurn(route, 1)
      .TestValid()
      .TestDirection(TurnDirection::UTurnLeft);
  integration::GetNthTurn(route, 2).TestValid().TestDirection(TurnDirection::TurnLeft);

  integration::TestRouteLength(route, 561.);
}

UNIT_TEST(StPetersburgSideRoadPenaltyTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(),
      MercatorBounds::FromLatLon(59.85157, 30.28033), {0., 0.},
      MercatorBounds::FromLatLon(59.84268, 30.27589));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;
  TEST_EQUAL(result, IRouter::NoError, ());

  integration::TestTurnCount(route, 0);
}

UNIT_TEST(RussiaMoscowLenigradskiy39UturnTurnTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(),
      MercatorBounds::FromLatLon(55.79693, 37.53754), {0., 0.},
      MercatorBounds::FromLatLon(55.80212, 37.5389));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;
  TEST_EQUAL(result, IRouter::NoError, ());

  integration::TestTurnCount(route, 3);

  integration::GetNthTurn(route, 0)
      .TestValid()
      .TestDirection(TurnDirection::UTurnLeft);
  integration::GetNthTurn(route, 1)
      .TestValid()
      .TestDirection(TurnDirection::TurnRight);
  integration::GetNthTurn(route, 2).TestValid().TestDirection(TurnDirection::TurnLeft);

  integration::TestRouteLength(route, 2033.);
}

UNIT_TEST(RussiaMoscowSalameiNerisUturnTurnTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(),
      MercatorBounds::FromLatLon(55.85182, 37.39533), {0., 0.},
      MercatorBounds::FromLatLon(55.84386, 37.39250));
  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 4);
  integration::GetNthTurn(route, 0)
      .TestValid()
      .TestPoint({37.38848, 67.63338}, 20.)
      .TestDirection(TurnDirection::TurnSlightRight);
  integration::GetNthTurn(route, 1)
      .TestValid()
      .TestPoint({37.38711, 67.63336}, 20.)
      .TestDirection(TurnDirection::TurnLeft);
  integration::GetNthTurn(route, 2)
      .TestValid()
      .TestPoint({37.38738, 67.63278}, 20.)
      .TestDirection(TurnDirection::TurnLeft);
  integration::GetNthTurn(route, 3)
      .TestValid()
      .TestPoint({37.39052, 67.63310}, 20.)
      .TestDirection(TurnDirection::TurnRight);

  integration::TestRouteLength(route, 1637.);
}

UNIT_TEST(RussiaMoscowTrikotagniAndPohodniRoundaboutTurnTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
    integration::GetOsrmComponents(),
    MercatorBounds::FromLatLon(55.83118, 37.40515), {0., 0.},
    MercatorBounds::FromLatLon(55.83384, 37.40521));
  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 2);

  integration::GetNthTurn(route, 0)
      .TestValid()
      .TestDirection(TurnDirection::EnterRoundAbout)
      .TestRoundAboutExitNum(2);
  integration::GetNthTurn(route, 1).TestValid().TestDirection(TurnDirection::LeaveRoundAbout);

  integration::TestRouteLength(route, 387.);
}

UNIT_TEST(SwedenBarlangeRoundaboutTurnTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(), MercatorBounds::FromLatLon(60.48278, 15.42356), {0., 0.},
      MercatorBounds::FromLatLon(60.48462, 15.42120));
  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 2);

  integration::GetNthTurn(route, 0)
      .TestValid()
      .TestDirection(TurnDirection::EnterRoundAbout)
      .TestRoundAboutExitNum(2);
  integration::GetNthTurn(route, 1).TestValid().TestDirection(TurnDirection::LeaveRoundAbout);

  integration::TestRouteLength(route, 255.);
}

UNIT_TEST(RussiaMoscowPlanetnaiTurnTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(),
      MercatorBounds::FromLatLon(55.80216, 37.54668), {0., 0.},
      MercatorBounds::FromLatLon(55.80169, 37.54915));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 1);
  integration::GetNthTurn(route, 0).TestValid().TestDirection(TurnDirection::TurnLeft);

  integration::TestRouteLength(route, 214.);
}

UNIT_TEST(RussiaMoscowNoTurnsOnMKADTurnTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(),
      MercatorBounds::FromLatLon(55.84656, 37.39163), {0., 0.},
      MercatorBounds::FromLatLon(55.56661, 37.69254));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 1);
  integration::GetNthTurn(route, 0)
      .TestValid()
      .TestPoint({37.68276, 67.14062})
      .TestOneOfDirections({TurnDirection::TurnSlightRight, TurnDirection::TurnRight});

  integration::TestRouteLength(route, 43233.7);
}

UNIT_TEST(RussiaMoscowTTKKashirskoeShosseOutTurnTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(),
      MercatorBounds::FromLatLon(55.70160, 37.60632), {0., 0.},
      MercatorBounds::FromLatLon(55.69349, 37.62122));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 1);
  // Checking a turn in case going from a not-link to a link
  integration::GetNthTurn(route, 0).TestValid().TestOneOfDirections(
      {TurnDirection::TurnSlightRight, TurnDirection::TurnRight});
}

UNIT_TEST(RussiaMoscowSchelkovskoeShosseUTurnTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(),
      MercatorBounds::FromLatLon(55.80967, 37.78037), {0., 0.},
      MercatorBounds::FromLatLon(55.80955, 37.78056));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 1);
  // Checking a turn in case going from a not-link to a link
  integration::GetNthTurn(route, 0).TestValid().TestDirection(TurnDirection::UTurnLeft);
}

UNIT_TEST(RussiaMoscowParallelResidentalUTurnAvoiding)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(),
      MercatorBounds::FromLatLon(55.66192, 37.62852), {0., 0.},
      MercatorBounds::FromLatLon(55.66189, 37.63254));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 2);
  // Checking a turn in case going from a not-link to a link
  integration::GetNthTurn(route, 0).TestValid().TestDirection(TurnDirection::TurnLeft);
  integration::GetNthTurn(route, 1).TestValid().TestDirection(TurnDirection::TurnLeft);
}

UNIT_TEST(RussiaMoscowPankratevskiPerBolshaySuharedskazPloschadTurnTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(),
      MercatorBounds::FromLatLon(55.77177, 37.63556), {0., 0.},
      MercatorBounds::FromLatLon(55.77203, 37.63705));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 1);
  integration::GetNthTurn(route, 0).TestValid().TestDirection(TurnDirection::TurnRight);
}

UNIT_TEST(RussiaMoscowMKADPutilkovskeShosseTurnTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(),
      MercatorBounds::FromLatLon(55.85305, 37.39414), {0., 0.},
      MercatorBounds::FromLatLon(55.85099, 37.39105));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 1);
  integration::GetNthTurn(route, 0).TestValid().TestOneOfDirections(
      {TurnDirection::TurnSlightRight, TurnDirection::TurnRight});
}

UNIT_TEST(RussiaMoscowPetushkovaShodniaReverTurnTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(),
      MercatorBounds::FromLatLon(55.84104, 37.40591), {0., 0.},
      MercatorBounds::FromLatLon(55.83929, 37.40855));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 0);
}

UNIT_TEST(RussiaHugeRoundaboutTurnTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(),
      MercatorBounds::FromLatLon(55.80141, 37.32581), {0., 0.},
      MercatorBounds::FromLatLon(55.80075, 37.32536));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 2);
  integration::GetNthTurn(route, 0)
      .TestValid()
      .TestDirection(TurnDirection::EnterRoundAbout)
      .TestRoundAboutExitNum(5);
  integration::GetNthTurn(route, 1)
      .TestValid()
      .TestDirection(TurnDirection::LeaveRoundAbout)
      .TestRoundAboutExitNum(5);
}

UNIT_TEST(BelarusMiskProspNezavisimostiMKADTurnTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(),
      MercatorBounds::FromLatLon(53.93642, 27.65857), {0., 0.},
      MercatorBounds::FromLatLon(53.93933, 27.67046));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 1);
  integration::GetNthTurn(route, 0).TestValid().TestOneOfDirections(
      {TurnDirection::TurnSlightRight, TurnDirection::TurnRight});
}

// Test case: turning form one street to another one with the same name.
// An end user shall be informed about this manoeuvre.
UNIT_TEST(RussiaMoscowPetushkovaPetushkovaTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(),
      MercatorBounds::FromLatLon(55.83636, 37.40555), {0., 0.},
      MercatorBounds::FromLatLon(55.83707, 37.40489));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 1);
  integration::GetNthTurn(route, 0).TestValid().TestDirection(TurnDirection::TurnLeft);
}

// Test case: a route goes straight along a unnamed big link road when joined a small road.
// An end user shall not be informed about such manoeuvres.
UNIT_TEST(RussiaMoscowMKADLeningradkaTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(), MercatorBounds::FromLatLon(55.87992, 37.43940),
      {0., 0.}, MercatorBounds::FromLatLon(55.87854, 37.44865));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 1);
}

UNIT_TEST(BelarusMKADShosseinai)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(),
      MercatorBounds::FromLatLon(55.31541, 29.43123), {0., 0.},
      MercatorBounds::FromLatLon(55.31656, 29.42626));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 1);
  integration::GetNthTurn(route, 0).TestValid().TestOneOfDirections(
      {TurnDirection::GoStraight, TurnDirection::TurnSlightRight});
}

// Test case: a route goes straight along a unnamed big road when joined small road.
// An end user shall not be informed about such manoeuvres.
UNIT_TEST(ThailandPhuketNearPrabarameeRoad)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(),
      MercatorBounds::FromLatLon(7.91797, 98.36937), {0., 0.},
      MercatorBounds::FromLatLon(7.90724, 98.36785));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 0);
}

// Test case: a route goes in Moscow from Varshavskoe shosse (from the city center direction)
// to MKAD (the outer side). A turn instruction (to leave Varshavskoe shosse)
// shall be generated.
UNIT_TEST(RussiaMoscowVarshavskoeShosseMKAD)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(), MercatorBounds::FromLatLon(55.58210, 37.59695), {0., 0.},
      MercatorBounds::FromLatLon(55.57514, 37.61020));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 1);
  integration::GetNthTurn(route, 0).TestValid().TestOneOfDirections(
      {TurnDirection::TurnSlightRight, TurnDirection::TurnRight});
}

// Test case: a route goes in Moscow from Tverskaya street (towards city center)
// to Mokhovaya street. A turn instruction (turn left) shell be generated.
UNIT_TEST(RussiaMoscowTverskajaOkhotnyRyadTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(), MercatorBounds::FromLatLon(55.75765, 37.61355), {0., 0.},
      MercatorBounds::FromLatLon(55.75737, 37.61601));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 1);
  integration::GetNthTurn(route, 0).TestValid().TestDirection(TurnDirection::TurnLeft);
}

UNIT_TEST(RussiaMoscowBolshoyKislovskiyPerBolshayaNikitinskayaUlTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(), MercatorBounds::FromLatLon(55.75574, 37.60702), {0., 0.},
      MercatorBounds::FromLatLon(55.75586, 37.60819));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 1);
  integration::GetNthTurn(route, 0).TestValid().TestDirection(TurnDirection::TurnRight);
}

// Test case: a route goes in Moscow along Leningradskiy Prpt (towards city center)
// and makes u-turn. A only one turn instruction (turn left) shell be generated.
UNIT_TEST(RussiaMoscowLeningradskiyPrptToTheCenterUTurnTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(), MercatorBounds::FromLatLon(55.79231, 37.54951), {0., 0.},
      MercatorBounds::FromLatLon(55.79280, 37.55028));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 1);
  integration::GetNthTurn(route, 0).TestValid().TestDirection(TurnDirection::UTurnLeft);
}

// Test case: checking that no unnecessary turn on a serpentine road.
// This test was written after reducing factors kMaxPointsCount and kMinDistMeters.
UNIT_TEST(SwitzerlandSamstagernBergstrasseTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(), MercatorBounds::FromLatLon(47.19300, 8.67568), {0., 0.},
      MercatorBounds::FromLatLon(47.19162, 8.67590));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 0);
}

UNIT_TEST(RussiaMoscowMikoiankNoUTurnTest)
{
  TRouteResult const routeResult = integration::CalculateRoute(
      integration::GetOsrmComponents(), MercatorBounds::FromLatLon(55.79041, 37.53770), {0., 0.},
      MercatorBounds::FromLatLon(55.79182, 37.53008));

  Route const & route = *routeResult.first;
  IRouter::ResultCode const result = routeResult.second;

  TEST_EQUAL(result, IRouter::NoError, ());
  integration::TestTurnCount(route, 0);
}
