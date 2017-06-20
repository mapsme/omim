#pragma once

#include "routing/num_mwm_id.hpp"
#include "routing/subway_vertex.hpp"

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

  SubwayFeature(std::vector<m2::PointD> && points, std::string color, SubwayType type)
    : m_points(points), m_color(color), m_type(type)
  {
  }

  std::vector<m2::PointD> const & GetPoints() const { return m_points; }
  std::string const & GetColor() { return m_color; }
  SubwayType GetType() const { return m_type; }

private:
  std::vector<m2::PointD> m_points;
  std::string m_color;
  SubwayType m_type = SubwayType::Line;
};

class SubwayCache final
{
public:
  SubwayCache(std::shared_ptr<NumMwmIds> numMwmIds, Index & index);

  SubwayFeature const & GetFeature(NumMwmId numMwmId, uint32_t featureId);

private:
  std::shared_ptr<NumMwmIds> m_numMwmIds;
  Index & m_index;
  std::unordered_map<NumMwmId, std::unordered_map<uint32_t, SubwayFeature>> m_features;
};
}  // namespace routing
