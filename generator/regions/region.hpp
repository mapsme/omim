#pragma once

#include "generator/feature_builder.hpp"
#include "generator/regions/region_base.hpp"
#include "generator/regions/place_point.hpp"

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
// This is a helper class that is needed to represent the region.
// With this view, further processing is simplified.
class Region : protected RegionWithName, protected RegionWithData
{
public:
  explicit Region(feature::FeatureBuilder const & fb, RegionDataProxy const & rd);
  // Build a region and its boundary based on the heuristic.
  explicit Region(PlacePoint const & place);

  // See RegionWithName::GetEnglishOrTransliteratedName().
  std::string GetEnglishOrTransliteratedName() const;
  std::string GetName(int8_t lang = StringUtf8Multilang::kDefaultCode) const;

  base::GeoObjectId GetId() const;
  using RegionWithData::GetAdminLevel;
  PlaceType GetPlaceType() const;
  boost::optional<std::string> GetIsoCode() const;

  using RegionWithData::GetLabelOsmId;
  void SetLabel(PlacePoint const & place);

  bool Contains(Region const & smaller) const;
  bool ContainsRect(Region const & smaller) const;
  bool Contains(PlacePoint const & place) const;
  bool Contains(BoostPoint const & point) const;
  double CalculateOverlapPercentage(Region const & other) const;
  BoostPoint GetCenter() const;
  bool IsLocality() const;
  BoostRect const & GetRect() const { return m_rect; }
  std::shared_ptr<BoostPolygon> const & GetPolygon() const noexcept { return m_polygon; }
  double GetArea() const { return m_area; }
  // This function uses heuristics and assigns a radius according to the tag place.
  // The radius will be returned in mercator units.
  static double GetRadiusByPlaceType(PlaceType place);

private:
  void FillPolygon(feature::FeatureBuilder const & fb);

  boost::optional<PlacePoint> m_placeLabel;
  std::shared_ptr<BoostPolygon> m_polygon;
  BoostRect m_rect;
  double m_area;
};

std::string GetRegionNotation(Region const & region);
}  // namespace regions
}  // namespace generator
