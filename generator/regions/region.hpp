#pragma once

#include "generator/feature_builder.hpp"
#include "generator/regions/region_base.hpp"

#include <memory>

class FeatureBuilder1;

namespace generator
{
class RegionDataProxy;

namespace regions
{
class PlaceCenter;

// This is a helper class that is needed to represent the region.
// With this view, further processing is simplified.
class Region : public RegionWithName, public RegionWithData
{
public:
  Region(FeatureBuilder1 const & fb, RegionDataProxy const & rd);
  Region(StringUtf8Multilang const & name, RegionDataProxy const & rd,
         std::shared_ptr<BoostPolygon> const & polygon);

  // After calling DeletePolygon, you cannot use Contains, ContainsRect, CalculateOverlapPercentage.
  void DeletePolygon();
  bool Contains(Region const & smaller) const;
  bool ContainsRect(Region const & smaller) const;
  bool Contains(PlaceCenter const & placePoint) const;
  bool Contains(BoostPoint const & point) const;
  double CalculateOverlapPercentage(Region const & other) const;
  BoostPoint GetCenter() const;
  BoostRect const & GetRect() const { return m_rect; }
  std::shared_ptr<BoostPolygon> const & GetPolygon() const noexcept { return m_polygon; }
  double GetArea() const { return m_area; }

private:
  void FillPolygon(FeatureBuilder1 const & fb);

  std::shared_ptr<BoostPolygon> m_polygon;
  BoostRect m_rect;
  double m_area;
};
}  // namespace regions
}  // namespace generator
