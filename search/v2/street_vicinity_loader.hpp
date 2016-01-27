#pragma once

#include "search/projection_on_street.hpp"
#include "search/v2/mwm_context.hpp"
#include "search/v2/stats_cache.hpp"

#include "indexer/feature.hpp"
#include "indexer/feature_algo.hpp"

#include "geometry/rect2d.hpp"

#include "base/macros.hpp"

#include "std/unordered_map.hpp"


namespace search
{
namespace v2
{
struct MwmContext;

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

  StreetVicinityLoader(int scale, double offsetMeters);
  void SetContext(MwmContext * context);

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
      m_context->m_vector.GetByIndex(id, ft);
      if (!calculator.GetProjection(feature::GetCenter(ft, FeatureType::WORST_GEOMETRY), proj))
        continue;

      fn(id);
    }
  }

  void OnQueryFinished();

  Street const & GetStreet(uint32_t featureId);

private:
  void LoadStreet(uint32_t featureId, Street & street);

  MwmContext * m_context;
  int m_scale;
  double const m_offsetMeters;

  Cache<uint32_t, Street> m_cache;

  DISALLOW_COPY_AND_MOVE(StreetVicinityLoader);
};
}  // namespace v2
}  // namespace search
