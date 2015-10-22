#include "drape_frontend/map_data_provider.hpp"

namespace df
{

MapDataProvider::MapDataProvider(TReadIDsFn const & idsReader,
                                 TReadFeaturesFn const & featureReader,
                                 TResolveCountryFn const & countryResolver)
  : m_featureReader(featureReader)
  , m_idsReader(idsReader)
  , m_countryResolver(countryResolver)
{
}

void MapDataProvider::ReadFeaturesID(TReadIdCallback const & fn, m2::RectD const & r, int scale) const
{
  m_idsReader(fn, r, scale);
}

void MapDataProvider::ReadFeatures(TReadFeatureCallback const & fn, vector<FeatureID> const & ids) const
{
  m_featureReader(fn, ids);
}

storage::TIndex MapDataProvider::FindCountry(m2::PointF const & pt)
{
  return m_countryResolver(pt);
}

}
