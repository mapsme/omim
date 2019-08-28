#include "routing/edge_estimator.hpp"

#include "routing/routing_helpers.hpp"

#include "traffic/speed_groups.hpp"
#include "traffic/traffic_info.hpp"

#include "indexer/feature_altitude.hpp"

#include "base/assert.hpp"

#include <algorithm>
#include <unordered_map>

using namespace routing;
using namespace std;
using namespace traffic;

namespace
{
feature::TAltitude constexpr kMountainSicknessAltitudeM = 2500;

double TimeBetweenSec(m2::PointD const & from, m2::PointD const & to, double speedMpS)
{
  CHECK_GREATER(speedMpS, 0.0,
                ("from:", MercatorBounds::ToLatLon(from), "to:", MercatorBounds::ToLatLon(to)));

  double const distanceM = MercatorBounds::DistanceOnEarth(from, to);
  return distanceM / speedMpS;
}

double CalcTrafficFactor(SpeedGroup speedGroup)
{
  if (speedGroup == SpeedGroup::TempBlock)
  {
    double constexpr kImpossibleDrivingFactor = 1e4;
    return kImpossibleDrivingFactor;
  }

  double const percentage =
      0.01 * static_cast<double>(kSpeedGroupThresholdPercentage[static_cast<size_t>(speedGroup)]);
  CHECK_GREATER(percentage, 0.0, ("Speed group:", speedGroup));
  return 1.0 / percentage;
}

double GetPedestrianClimbPenalty(double tangent, feature::TAltitude altitudeM)
{
  if (tangent <= 0) // Descent
    return 1.0 + 2.0 * (-tangent);

  // Climb.
  // The upper the penalty is more:
  // |1 + 10 * tangent| for altitudes lower than |kMountainSicknessAltitudeM|
  // |1 + 20 * tangent| for 4000 meters
  // |1 + 30 * tangent| for 5500 meters
  // |1 + 40 * tangent| for 7000 meters
  return 1.0 + (10.0 + max(0, altitudeM - kMountainSicknessAltitudeM) * 10.0 / 1500) * tangent;
}

double GetBicycleClimbPenalty(double tangent, feature::TAltitude altitudeM)
{
  if (tangent <= 0) // Descent
    return 1.0;

  // Climb.
  if (altitudeM < kMountainSicknessAltitudeM)
    return 1.0 + 30.0 * tangent;

  return 1.0 + 50.0 * tangent;
}

double GetCarClimbPenalty(double /* tangent */, feature::TAltitude /* altitude */) { return 1.0; }

template <typename GetClimbPenalty>
double CalcClimbSegment(EdgeEstimator::Purpose purpose, Segment const & segment,
                        RoadGeometry const & road, GetClimbPenalty && getClimbPenalty)
{
  Junction const & from = road.GetJunction(segment.GetPointId(false /* front */));
  Junction const & to = road.GetJunction(segment.GetPointId(true /* front */));
  SpeedKMpH const & speed = road.GetSpeed(segment.IsForward());

  double const distance = MercatorBounds::DistanceOnEarth(from.GetPoint(), to.GetPoint());
  double const speedMpS = KMPH2MPS(purpose == EdgeEstimator::Purpose::Weight ? speed.m_weight : speed.m_eta);
  CHECK_GREATER(speedMpS, 0.0, ());
  double const timeSec = distance / speedMpS;

  if (base::AlmostEqualAbs(distance, 0.0, 0.1))
    return timeSec;

  double const altitudeDiff =
      static_cast<double>(to.GetAltitude()) - static_cast<double>(from.GetAltitude());
  return timeSec * getClimbPenalty(altitudeDiff / distance, to.GetAltitude());
}
}  // namespace

namespace routing
{
// EdgeEstimator -----------------------------------------------------------------------------------
EdgeEstimator::EdgeEstimator(double maxWeightSpeedKMpH, double offroadSpeedKMpH)
  : m_maxWeightSpeedMpS(KMPH2MPS(maxWeightSpeedKMpH)), m_offroadSpeedMpS(KMPH2MPS(offroadSpeedKMpH))
{
  CHECK_GREATER(m_offroadSpeedMpS, 0.0, ());
  CHECK_GREATER_OR_EQUAL(m_maxWeightSpeedMpS, m_offroadSpeedMpS, ());
}

double EdgeEstimator::CalcHeuristic(m2::PointD const & from, m2::PointD const & to) const
{
  return TimeBetweenSec(from, to, m_maxWeightSpeedMpS);
}

double EdgeEstimator::CalcLeapWeight(m2::PointD const & from, m2::PointD const & to) const
{
  // Let us assume for the time being that
  // leap edges will be added with a half of max speed.
  // @TODO(bykoianko) It's necessary to gather statistics to calculate better factor(s) instead of
  // one below.
  return TimeBetweenSec(from, to, m_maxWeightSpeedMpS / 2.0);
}

double EdgeEstimator::CalcOffroadWeight(m2::PointD const & from, m2::PointD const & to) const
{
  return TimeBetweenSec(from, to, m_offroadSpeedMpS);
}

// PedestrianEstimator -----------------------------------------------------------------------------
class PedestrianEstimator final : public EdgeEstimator
{
public:
  PedestrianEstimator(double maxWeightSpeedKMpH, double offroadSpeedKMpH)
    : EdgeEstimator(maxWeightSpeedKMpH, offroadSpeedKMpH)
  {
  }

  // EdgeEstimator overrides:
  double GetUTurnPenalty(Purpose /* purpose */) const override { return 0.0 /* seconds */; }
  // Based on: https://confluence.mail.ru/display/MAPSME/Ferries
  double GetFerryLandingPenalty(Purpose purpose) const override
  {
    switch (purpose)
    {
    case Purpose::Weight: return 20.0 * 60.0;  // seconds
    case Purpose::ETA: return 8.0 * 60.0;  // seconds
    }
    UNREACHABLE();
  }

  double CalcSegmentWeight(Segment const & segment, RoadGeometry const & road) const override
  {
    return CalcClimbSegment(Purpose::Weight, segment, road, GetPedestrianClimbPenalty);
  }

  double CalcSegmentETA(Segment const & segment, RoadGeometry const & road) const override
  {
    return CalcClimbSegment(Purpose::ETA, segment, road, GetPedestrianClimbPenalty);
  }
};

// BicycleEstimator --------------------------------------------------------------------------------
class BicycleEstimator final : public EdgeEstimator
{
public:
  BicycleEstimator(double maxWeightSpeedKMpH, double offroadSpeedKMpH)
    : EdgeEstimator(maxWeightSpeedKMpH, offroadSpeedKMpH)
  {
  }

  // EdgeEstimator overrides:
  double GetUTurnPenalty(Purpose /* purpose */) const override { return 20.0 /* seconds */; }
  // Based on: https://confluence.mail.ru/display/MAPSME/Ferries
  double GetFerryLandingPenalty(Purpose purpose) const override
  {
    switch (purpose)
    {
    case Purpose::Weight: return 20 * 60;  // seconds
    case Purpose::ETA: return 8 * 60;  // seconds
    }
    UNREACHABLE();
  }

  double CalcSegmentWeight(Segment const & segment, RoadGeometry const & road) const override
  {
    return CalcClimbSegment(Purpose::Weight, segment, road, GetBicycleClimbPenalty);
  }

  double CalcSegmentETA(Segment const & segment, RoadGeometry const & road) const override
  {
    return CalcClimbSegment(Purpose::ETA, segment, road, GetBicycleClimbPenalty);
  }
};

// CarEstimator ------------------------------------------------------------------------------------
class CarEstimator final : public EdgeEstimator
{
public:
  CarEstimator(shared_ptr<TrafficStash> trafficStash, double maxWeightSpeedKMpH,
               double offroadSpeedKMpH);

  // EdgeEstimator overrides:
  double CalcSegmentWeight(Segment const & segment, RoadGeometry const & road) const override;
  double CalcSegmentETA(Segment const & segment, RoadGeometry const & road) const override;
  double GetUTurnPenalty(Purpose /* purpose */) const override;
  double GetFerryLandingPenalty(Purpose purpose) const override;

private:
  double CalcSegment(Purpose purpose, Segment const & segment, RoadGeometry const & road) const;
  shared_ptr<TrafficStash> m_trafficStash;
};

CarEstimator::CarEstimator(shared_ptr<TrafficStash> trafficStash, double maxWeightSpeedKMpH,
                           double offroadSpeedKMpH)
  : EdgeEstimator(maxWeightSpeedKMpH, offroadSpeedKMpH), m_trafficStash(move(trafficStash))
{
}

double CarEstimator::CalcSegmentWeight(Segment const & segment, RoadGeometry const & road) const
{
  return CalcSegment(Purpose::Weight, segment, road);
}

double CarEstimator::CalcSegmentETA(Segment const & segment, RoadGeometry const & road) const
{
  return CalcSegment(Purpose::ETA, segment, road);
}

double CarEstimator::GetUTurnPenalty(Purpose /* purpose */) const
{
  // Adds 2 minutes penalty for U-turn. The value is quite arbitrary
  // and needs to be properly selected after a number of real-world
  // experiments.
  return 2 * 60;  // seconds
}

double CarEstimator::GetFerryLandingPenalty(Purpose purpose) const
{
  switch (purpose)
  {
  case Purpose::Weight: return 40 * 60;  // seconds
  // Based on https://confluence.mail.ru/display/MAPSME/Ferries
  case Purpose::ETA: return 20 * 60;  // seconds
  }
  UNREACHABLE();
}

double CarEstimator::CalcSegment(Purpose purpose, Segment const & segment, RoadGeometry const & road) const
{
  double result = CalcClimbSegment(purpose, segment, road, GetCarClimbPenalty);

  if (m_trafficStash)
  {
    SpeedGroup const speedGroup = m_trafficStash->GetSpeedGroup(segment);
    ASSERT_LESS(speedGroup, SpeedGroup::Count, ());
    double const trafficFactor = CalcTrafficFactor(speedGroup);
    result *= trafficFactor;
    if (speedGroup != SpeedGroup::Unknown && speedGroup != SpeedGroup::G5)
    {
      // Current time estimation are too optimistic.
      // Need more accurate tuning: traffic lights, traffic jams, road models and so on.
      // Add some penalty to make estimation of a more realistic.
      // TODO: make accurate tuning, remove penalty.
      double constexpr kTimePenalty = 1.8;
      result *= kTimePenalty;
    }
  }

  return result;
}

// EdgeEstimator -----------------------------------------------------------------------------------
// static
shared_ptr<EdgeEstimator> EdgeEstimator::Create(VehicleType vehicleType, double maxWeighSpeedKMpH,
                                                double offroadSpeedKMpH,
                                                shared_ptr<TrafficStash> trafficStash)
{
  switch (vehicleType)
  {
  case VehicleType::Pedestrian:
  case VehicleType::Transit:
    return make_shared<PedestrianEstimator>(maxWeighSpeedKMpH, offroadSpeedKMpH);
  case VehicleType::Bicycle:
    return make_shared<BicycleEstimator>(maxWeighSpeedKMpH, offroadSpeedKMpH);
  case VehicleType::Car:
    return make_shared<CarEstimator>(trafficStash, maxWeighSpeedKMpH, offroadSpeedKMpH);
  case VehicleType::Count:
    CHECK(false, ("Can't create EdgeEstimator for", vehicleType));
    return nullptr;
  }
  UNREACHABLE();
}

// static
shared_ptr<EdgeEstimator> EdgeEstimator::Create(VehicleType vehicleType,
                                                VehicleModelInterface const & vehicleModel,
                                                shared_ptr<TrafficStash> trafficStash)
{
  return Create(vehicleType, vehicleModel.GetMaxWeightSpeed(), vehicleModel.GetOffroadSpeed(),
                trafficStash);
}
}  // namespace routing
