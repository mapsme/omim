#pragma once

#include "partners_api/promo_api.hpp"

#include "indexer/feature_to_osm.hpp"

#include "geometry/point2d.hpp"

#include <memory>
#include <string>

class DataSource;

namespace search
{
class CityFinder;
}

class PromoDelegate : public promo::Api::Delegate
{
public:
  PromoDelegate(DataSource const & dataSource, search::CityFinder & cityFinder);

  std::string GetCityId(m2::PointD const & point) override;

private:
  DataSource const & m_dataSource;
  search::CityFinder & m_cityFinder;
  std::unique_ptr<indexer::FeatureIdToGeoObjectIdBimap> m_cities;
};
