#pragma once

#include "routing/num_mwm_id.hpp"
#include "routing/subway_vertex.hpp"

#include "routing_common/vehicle_model.hpp"

#include "indexer/index.hpp"

#include "geometry/point2d.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace routing
{
class SubwayFeature final
{
public:
  SubwayFeature() = default;

  SubwayFeature(SubwayType type, std::string const & color, std::vector<m2::PointD> && points,
                double speedKmPerH)
    : m_points(std::move(points)), m_color(color), m_type(type), m_speedKmPerH(speedKmPerH)
  {
  }

  std::vector<m2::PointD> const & GetPoints() const { return m_points; }
  std::string const & GetColor() const { return m_color; }
  SubwayType GetType() const { return m_type; }
  double GetSpeed() const { return m_speedKmPerH; }

private:
  std::vector<m2::PointD> m_points;
  std::string m_color;
  SubwayType m_type = SubwayType::Line;
  double m_speedKmPerH;
};

class SubwayCache final
{
public:
  SubwayCache(std::shared_ptr<NumMwmIds> numMwmIds,
              std::shared_ptr<VehicleModelFactory> modelFactory, Index & index);

  SubwayFeature const & GetFeature(NumMwmId numMwmId, uint32_t featureId);

private:
  std::shared_ptr<NumMwmIds> m_numMwmIds;
  std::shared_ptr<VehicleModelFactory> m_modelFactory;
  Index & m_index;
  std::unordered_map<NumMwmId, std::unordered_map<uint32_t, SubwayFeature>> m_features;
};
}  // namespace routing
