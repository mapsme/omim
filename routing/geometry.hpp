#pragma once

#include "routing/base/astar_weight.hpp"

#include "routing/road_point.hpp"
#include "routing/road_graph.hpp"

#include "routing_common/vehicle_model.hpp"

#include "indexer/feature_altitude.hpp"
#include "indexer/index.hpp"

#include "geometry/point2d.hpp"

#include "base/buffer_vector.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

namespace routing
{
class RoadGeometry final
{
public:
  using Points = buffer_vector<m2::PointD, 32>;

  RoadGeometry() = default;
  RoadGeometry(bool oneWay, double speed, Points const & points);
  ~RoadGeometry()
  {
    return;
  }

  void Load(VehicleModelInterface const & vehicleModel, FeatureType const & feature,
            feature::TAltitudes const * altitudes);

  bool IsOneWay() const { return m_isOneWay; }
  // Kilometers per hour.
  double GetSpeed() const { return m_speed; }
  bool IsPassThroughAllowed() const { return m_isPassThroughAllowed; }

  Junction const & GetJunction(uint32_t junctionId) const
  {
    ASSERT_LESS(junctionId, m_junctions.size(), ());
    return m_junctions[junctionId];
  }

  m2::PointD const & GetPoint(uint32_t pointId) const { return GetJunction(pointId).GetPoint(); }

  uint32_t GetPointsCount() const { return static_cast<uint32_t>(m_junctions.size()); }

  // Note. It's possible that car_model was changed after the map was built.
  // For example, the map from 12.2016 contained highway=pedestrian
  // in car_model but this type of highways is removed as of 01.2017.
  // In such cases RoadGeometry is not valid.
  bool IsValid() const { return m_valid; }

  bool IsEndPointId(uint32_t pointId) const
  {
    ASSERT_LESS(pointId, m_junctions.size(), ());
    return pointId == 0 || pointId + 1 == GetPointsCount();
  }

  void SetPassThroughAllowedForTests(bool passThroughAllowed)
  {
    m_isPassThroughAllowed = passThroughAllowed;
  }

  size_t GetSize() const
  {
    size_t sz = m_junctions.GetSize();
    return sz + sizeof(double) + 3 * sizeof(bool);
  }

private:
  buffer_vector<Junction, 32> m_junctions;
  double m_speed = 0.0;
  bool m_isOneWay = false;
  bool m_valid = false;
  bool m_isPassThroughAllowed = false;
};

class GeometryLoader
{
public:
  virtual ~GeometryLoader() = default;

  virtual void Load(uint32_t featureId, RoadGeometry & road) = 0;
  virtual size_t GetSize() const = 0;

  // handle should be alive: it is caller responsibility to check it.
  static std::unique_ptr<GeometryLoader> Create(Index const & index,
                                                MwmSet::MwmHandle const & handle,
                                                std::shared_ptr<VehicleModelInterface> vehicleModel,
                                                bool loadAltitudes);

  /// This is for stand-alone work.
  /// Use in generator_tool and unit tests.
  static std::unique_ptr<GeometryLoader> CreateFromFile(
      std::string const & fileName, std::shared_ptr<VehicleModelInterface> vehicleModel);
};

class B
{
public:
  ~B()
  {
    return;
  }
};

class С
{
public:
  ~С()
  {
    return;
  }
};

class F
{
public:
  ~F()
  {
    return;
  }
};
  
class Geometry final
{
public:
  Geometry()
  {
    m_roads = new std::map<uint32_t, RoadGeometry>();
  }
  explicit Geometry(std::unique_ptr<GeometryLoader> loader);
  ~Geometry()
  {
    LOG(LINFO, ("Geometry size:", GetSizeMB(GetSize()), "MB.", "m_roads size:", m_roads->size()));
    delete m_roads;
    LOG(LINFO, ("After deleting"));
  }

  RoadGeometry const & GetRoad(uint32_t featureId);

  m2::PointD const & GetPoint(RoadPoint const & rp)
  {
    return GetRoad(rp.GetFeatureId()).GetPoint(rp.GetPointId());
  }

  size_t GetSize() const
  {
    size_t rSz = 0;
    for (auto const & r : *m_roads)
      rSz += (sizeof(uint32_t) + r.second.GetSize());

    return rSz + m_loader->GetSize();
  }

private:
  // Feature id to RoadGeometry map.
  B b;
  std::unique_ptr<GeometryLoader> m_loader;
  С с;
  std::map<uint32_t, RoadGeometry> * m_roads = nullptr;
  F f;
};
}  // namespace routing
