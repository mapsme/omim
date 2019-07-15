#pragma once

#include "generator/cities_boundaries_builder.hpp"
#include "generator/feature_builder.hpp"
#include "generator/place.hpp"

#include "geometry/tree4d.hpp"

#include <memory>
#include <vector>

namespace generator
{
// The class CityBoundaryProcessor is responsible for the union of boundaries of the cities.
class CityBoundaryProcessor
{
public:
  CityBoundaryProcessor(std::shared_ptr<OsmIdToBoundariesTable> boundariesTable);

  void Add(feature::FeatureBuilder const & fb);
  void TryUpdate(feature::FeatureBuilder const & fb);
  std::vector<feature::FeatureBuilder> GetFeatures() const;

private:
  void UnionEqualPlacesIds(Place const & place);

  std::shared_ptr<OsmIdToBoundariesTable> m_boundariesTable;
  m4::Tree<Place> m_places;
};
} // namespace generator
