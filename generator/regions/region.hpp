#pragma once

#include "generator/feature_builder.hpp"
#include "generator/regions/region_base.hpp"

#include <memory>

namespace feature
{
class FeatureBuilder;
}  // namespace feature

namespace generator
{
class RegionDataProxy;

namespace regions
{
class PlacePoint;

// This is a helper class that is needed to represent the region.
// With this view, further processing is simplified.
class Region : public RegionWithName, public RegionWithData
{
public:
  explicit Region(feature::FeatureBuilder const & fb, RegionDataProxy const & rd);
  // Build a region and its boundary based on the heuristic.
  explicit Region(PlacePoint const & place);

  // After calling DeletePolygon, you cannot use Contains, ContainsRect, CalculateOverlapPercentage.
  void DeletePolygon();
  bool Contains(Region const & smaller) const;
  bool ContainsRect(Region const & smaller) const;
  bool Contains(PlacePoint const & place) const;
  bool Contains(BoostPoint const & point) const;
  double CalculateOverlapPercentage(Region const & other) const;
  BoostPoint GetCenter() const;
  bool IsCountry() const;
  bool IsLocality() const;
  BoostRect const & GetRect() const { return m_rect; }
  double GetArea() const { return m_area; }
  // This function uses heuristics and assigns a radius according to the tag place.
  // The radius will be returned in mercator units.
  static double GetRadiusByPlaceType(PlaceType place);

private:
  void FillPolygon(feature::FeatureBuilder const & fb);

  std::shared_ptr<BoostPolygon> m_polygon;
  BoostRect m_rect;
  double m_area;
};

bool FeaturePlacePointToRegion(RegionInfo const & regionInfo, feature::FeatureBuilder & feature);
}  // namespace regions
}  // namespace generator
