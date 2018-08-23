#pragma once

#include "indexer/city_boundary.hpp"
#include "indexer/data_source.hpp"

#include "geometry/point2d.hpp"
#include "geometry/tree4d.hpp"

#include <vector>

namespace routing
{
/// \brief Loading city and town boundaries based on cities_boundaries section of World.mwm
/// and checking if a point inside city or town.
class CityBoundariesChecker
{
  using CityBoundaries = std::vector<std::vector<indexer::CityBoundary>>;
public:
  explicit CityBoundariesChecker(DataSource const & dataSource);

  /// \returns true if |point| is inside a city or town according to cities_boundaries section
  /// of World.mwm and false otherwise.
  bool InCity(m2::PointD const & point) const;

  /// \returns number of convex polygons of city boundaries.
  size_t GetSizeForTesting() const { return m_tree.GetSize(); }

private:
  bool Load(DataSource const & dataSource, CityBoundaries & cityBoundaries);
  void FillTree(CityBoundaries const & cityBoundaries);

  m4::Tree<indexer::CityBoundary> m_tree;
};
}  // namespace routing
