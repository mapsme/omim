#pragma once

#include "storage/index.hpp"
#include "storage/storage_defines.hpp"

#include "indexer/feature.hpp"
#include "geometry/rect2d.hpp"

#include <functional>

namespace df
{

class MapDataProvider
{
public:
  template <typename T> using TReadCallback = std::function<void (T const &)>;
  using TReadFeaturesFn = std::function<void (TReadCallback<FeatureType> const & , vector<FeatureID> const &)>;
  using TReadIDsFn = std::function<void (TReadCallback<FeatureID> const & , m2::RectD const &, int)>;
  using TIsCountryLoadedFn = std::function<bool (m2::PointD const &)>;
  using TIsCountryLoadedByNameFn = std::function<bool (std::string const &)>;
  using TUpdateCurrentCountryFn = std::function<void (m2::PointD const &, int)>;

  MapDataProvider(TReadIDsFn const & idsReader,
                  TReadFeaturesFn const & featureReader,
                  TIsCountryLoadedByNameFn const & isCountryLoadedByNameFn,
                  TUpdateCurrentCountryFn const & updateCurrentCountryFn);

  void ReadFeaturesID(TReadCallback<FeatureID> const & fn, m2::RectD const & r, int scale) const;
  void ReadFeatures(TReadCallback<FeatureType> const & fn, vector<FeatureID> const & ids) const;

  TUpdateCurrentCountryFn const & UpdateCurrentCountryFn() const;

private:
  TReadFeaturesFn m_featureReader;
  TReadIDsFn m_idsReader;
  TUpdateCurrentCountryFn m_updateCurrentCountry;

public:
  TIsCountryLoadedByNameFn m_isCountryLoadedByName;
};

} // namespace df
