#pragma once

#include "routing/road_point.hpp"
#include "routing/vehicle_model.hpp"

#include "indexer/index.hpp"

#include "geometry/point2d.hpp"

#include "base/buffer_vector.hpp"

#include "std/cstdint.hpp"
#include "std/shared_ptr.hpp"
#include "std/string.hpp"
#include "std/unique_ptr.hpp"

namespace routing
{
class RoadGeometry final
{
public:
  using Points = buffer_vector<m2::PointD, 32>;

  RoadGeometry() = default;
  RoadGeometry(bool oneWay, double speed, Points const & points);

  void Load(IVehicleModel const & vehicleModel, FeatureType const & feature);

  bool IsOneWay() const { return m_isOneWay; }

  // Kilometers per hour.
  double GetSpeed() const { return m_speed; }

  m2::PointD const & GetPoint(uint32_t pointId) const
  {
    ASSERT_LESS(pointId, m_points.size(), ());
    return m_points[pointId];
  }

  uint32_t GetPointsCount() const { return m_points.size(); }
  bool operator==(RoadGeometry const & roadGeometry) const
  {
    return m_isOneWay == roadGeometry.m_isOneWay && m_speed == roadGeometry.m_speed &&
           m_points == roadGeometry.m_points;
  }

private:
  friend string DebugPrint(RoadGeometry const & roadGeometry);

  Points m_points;
  double m_speed = 0.0;
  bool m_isOneWay = false;
};

string DebugPrint(RoadGeometry const & roadGeometry);

class GeometryLoader
{
public:
  virtual ~GeometryLoader() = default;

  virtual void Load(uint32_t featureId, RoadGeometry & road) const = 0;

  // mwmId should be alive: it is caller responsibility to check it.
  static unique_ptr<GeometryLoader> Create(Index const & index, MwmSet::MwmId const & mwmId,
                                           shared_ptr<IVehicleModel> vehicleModel);
};

class Geometry final
{
public:
  Geometry() = default;
  explicit Geometry(unique_ptr<GeometryLoader> loader);

  RoadGeometry const & GetRoad(uint32_t featureId);

private:
  // Feature id to RoadGeometry map.
  unordered_map<uint32_t, RoadGeometry> m_roads;
  unique_ptr<GeometryLoader> m_loader;
};
}  // namespace routing
