#pragma once

#include "search/projection_on_street.hpp"

#include "indexer/feature.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/features_vector.hpp"
#include "indexer/scale_index.hpp"

#include "coding/reader.hpp"

#include "geometry/rect2d.hpp"

#include "base/macros.hpp"

#include "std/unordered_map.hpp"

class MwmValue;

namespace search
{
namespace v2
{
// This class is able to load features in a street's vicinity.
//
// NOTE: this class *IS NOT* thread-safe.
class StreetVicinityLoader
{
public:
  struct Street
  {
    Street() = default;
    Street(Street && street) = default;

    inline bool IsEmpty() const { return !m_calculator || m_rect.IsEmptyInterior(); }

    vector<uint32_t> m_features;
    m2::RectD m_rect;
    unique_ptr<ProjectionOnStreetCalculator> m_calculator;

    DISALLOW_COPY(Street);
  };

  StreetVicinityLoader(MwmValue & value, FeaturesVector const & featuresVector, int scale,
                       double offsetMeters);

  // Calls |fn| on each index in |sortedIds| where sortedIds[index]
  // belongs to the street's vicinity.
  template <typename TFn>
  void ForEachInVicinity(uint32_t streetId, vector<uint32_t> const & sortedIds, TFn const & fn)
  {
    Street const & street = GetStreet(streetId);
    if (street.IsEmpty())
      return;

    ProjectionOnStreetCalculator const & calculator = *street.m_calculator;
    ProjectionOnStreet proj;
    for (uint32_t id : street.m_features)
    {
      // Load center and check projection only when |id| is in |sortedIds|.
      if (!binary_search(sortedIds.begin(), sortedIds.end(), id))
        continue;

      FeatureType ft;
      m_featuresVector.GetByIndex(id, ft);
      if (!calculator.GetProjection(feature::GetCenter(ft, FeatureType::WORST_GEOMETRY), proj))
        continue;

      fn(id);
    }
  }

  Street const & GetStreet(uint32_t featureId);

  inline void ClearCache() { m_cache.clear(); }

private:
  void LoadStreet(uint32_t featureId, Street & street);

  ScaleIndex<ModelReaderPtr> m_index;
  FeaturesVector const & m_featuresVector;
  int m_scale;
  double const m_offsetMeters;

  unordered_map<uint32_t, Street> m_cache;

  DISALLOW_COPY_AND_MOVE(StreetVicinityLoader);
};
}  // namespace v2
}  // namespace search
