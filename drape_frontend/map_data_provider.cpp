#include "drape_frontend/map_data_provider.hpp"

namespace df
{

MapDataProvider::MapDataProvider(TReadIDsFn const & idsReader,
                                 TReadFeaturesFn const & featureReader,
                                 TUpdateCountryIndexFn const & countryIndexUpdater,
                                 TIsCountryLoadedFn const & isCountryLoadedFn,
                                 TIsCountryLoadedByNameFn const & isCountryLoadedByNameFn,
                                 TDownloadFn const & downloadMapHandler,
                                 TDownloadFn const & downloadRetryHandler,
                                 TDownloadFn const & downloadCancelHandler)
  : m_featureReader(featureReader)
  , m_idsReader(idsReader)
  , m_countryIndexUpdater(countryIndexUpdater)
  , m_isCountryLoadedFn(isCountryLoadedFn)
  , m_downloadMapHandler(downloadMapHandler)
  , m_downloadRetryHandler(downloadRetryHandler)
  , m_downloadCancelHandler(downloadCancelHandler)
  , m_isCountryLoadedByNameFn(isCountryLoadedByNameFn)
{
}

void MapDataProvider::ReadFeaturesID(TReadCallback<FeatureID> const & fn, m2::RectD const & r, int scale) const
{
  m_idsReader(fn, r, scale);
}

void MapDataProvider::ReadFeatures(TReadCallback<FeatureType> const & fn, vector<FeatureID> const & ids) const
{
  m_featureReader(fn, ids);
}

void MapDataProvider::UpdateCountryIndex(storage::TCountryId const & currentId, m2::PointF const & pt)
{
  m_countryIndexUpdater(currentId, pt);
}

MapDataProvider::TIsCountryLoadedFn const & MapDataProvider::GetIsCountryLoadedFn() const
{
  return m_isCountryLoadedFn;
}

TDownloadFn const & MapDataProvider::GetDownloadMapHandler() const
{
  return m_downloadMapHandler;
}

TDownloadFn const & MapDataProvider::GetDownloadRetryHandler() const
{
  return m_downloadRetryHandler;
}

TDownloadFn const & MapDataProvider::GetDownloadCancelHandler() const
{
  return m_downloadCancelHandler;
}

}
