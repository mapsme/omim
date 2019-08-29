#pragma once

#include "generator/cities_boundaries_builder.hpp"
#include "generator/feature_builder.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace generator
{
bool NeedProcessPlace(feature::FeatureBuilder const & fb);

// This structure encapsulates work with elements of different types.
// This means that we can consider a set of polygons of one relation as a single entity.
class FeaturePlace
{
public:
  using FeaturesBuilders = std::vector<feature::FeatureBuilder>;

  void Append(feature::FeatureBuilder const & fb);
  feature::FeatureBuilder const & GetFb() const;
  FeaturesBuilders const & GetFbs() const;
  m2::RectD const & GetLimitRect() const;
  uint8_t GetRank() const;
  std::string GetName() const;
  m2::PointD GetKeyPoint() const;
  StringUtf8Multilang const & GetMultilangName() const;
  bool IsPoint() const;

private:
  m2::RectD m_limitRect;
  FeaturesBuilders m_fbs;
  size_t m_bestIndex;
};

// The class PlaceProcessor is responsible for the union of boundaries of the places.
class PlaceProcessor
{
public:
  PlaceProcessor(std::shared_ptr<OsmIdToBoundariesTable> boundariesTable = {});

  void Add(feature::FeatureBuilder const & fb);
  std::vector<feature::FeatureBuilder> ProcessPlaces();

private:
  using FeaturePlaces = std::vector<FeaturePlace>;

  static std::string GetKey(feature::FeatureBuilder const & fb);
  void FillTable(FeaturePlaces::const_iterator start, FeaturePlaces::const_iterator end,
                 FeaturePlaces::const_iterator best) const;

  std::unordered_map<std::string, std::unordered_map<base::GeoObjectId, FeaturePlace>> m_nameToPlaces;
  std::shared_ptr<OsmIdToBoundariesTable> m_boundariesTable;
};
} // namespace generator
