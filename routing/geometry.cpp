#include "routing/geometry.hpp"

#include "routing/routing_exception.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"

#include "std/iostream.hpp"

using namespace routing;

namespace
{
class GeometryLoaderImpl final : public GeometryLoader
{
public:
  GeometryLoaderImpl(Index const & index, MwmSet::MwmId const & mwmId,
                     shared_ptr<IVehicleModel> vehicleModel);

  // GeometryLoader overrides:
  virtual void Load(uint32_t featureId, RoadGeometry & road) const override;

private:
  Index const & m_index;
  MwmSet::MwmId const m_mwmId;
  shared_ptr<IVehicleModel> m_vehicleModel;
  Index::FeaturesLoaderGuard m_guard;
};

GeometryLoaderImpl::GeometryLoaderImpl(Index const & index, MwmSet::MwmId const & mwmId,
                                       shared_ptr<IVehicleModel> vehicleModel)
  : m_index(index), m_mwmId(mwmId), m_vehicleModel(vehicleModel), m_guard(m_index, m_mwmId)
{
  ASSERT(m_vehicleModel, ());
}

void GeometryLoaderImpl::Load(uint32_t featureId, RoadGeometry & road) const
{
  FeatureType feature;
  bool const isFound = m_guard.GetFeatureByIndex(featureId, feature);
  if (!isFound)
    MYTHROW(RoutingException, ("Feature", featureId, "not found"));

  feature.ParseGeometry(FeatureType::BEST_GEOMETRY);
  road.Load(*m_vehicleModel, feature);
}
}  // namespace

namespace routing
{
// RoadGeometry ------------------------------------------------------------------------------------

RoadGeometry::RoadGeometry(bool oneWay, double speed, Points const & points)
  : m_isRoad(true), m_isOneWay(oneWay), m_speed(speed), m_points(points)
{
}

void RoadGeometry::Load(IVehicleModel const & vehicleModel, FeatureType const & feature)
{
  m_isRoad = vehicleModel.IsRoad(feature);
  m_isOneWay = vehicleModel.IsOneWay(feature);
  m_speed = vehicleModel.GetSpeed(feature);

  m_points.reserve(feature.GetPointsCount());
  for (size_t i = 0; i < feature.GetPointsCount(); ++i)
    m_points.emplace_back(feature.GetPoint(i));
}

// Geometry ----------------------------------------------------------------------------------------

Geometry::Geometry(unique_ptr<GeometryLoader> loader) : m_loader(move(loader))
{
  ASSERT(m_loader, ());
}

RoadGeometry const & Geometry::GetRoad(uint32_t featureId) const
{
  auto const & it = m_roads.find(featureId);
  if (it != m_roads.cend())
    return it->second;

  RoadGeometry & road = m_roads[featureId];
  m_loader->Load(featureId, road);
  return road;
}

string DebugPrint(RoadGeometry const & roadGeometry)
{
  stringstream str;
  str << "RoadGeometry [ m_isRoad: " << roadGeometry.m_isRoad
      << ", m_isOneWay: " << roadGeometry.m_isOneWay
      << ", m_speed: " << roadGeometry.m_speed
      << ", m_points: " << DebugPrint(roadGeometry.m_points)
      << " ]";
  return str.str();
}

unique_ptr<GeometryLoader> CreateGeometryLoader(Index const & index, MwmSet::MwmId const & mwmId,
                                                shared_ptr<IVehicleModel> vehicleModel)
{
  return make_unique<GeometryLoaderImpl>(index, mwmId, vehicleModel);
}
}  // namespace routing
