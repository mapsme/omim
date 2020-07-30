#include "map/catalog_headers_provider.hpp"

#include "map/bookmark_catalog.hpp"

#include <set>

CatalogHeadersProvider::CatalogHeadersProvider(PositionProvider const & positionProvider,
                                               storage::Storage const & storage)
  : m_positionProvider(positionProvider)
  , m_storage(storage)
{
}

void CatalogHeadersProvider::SetBookmarkCatalog(BookmarkCatalog const * bookmarkCatalog)
{
  ASSERT(bookmarkCatalog != nullptr, ());

  m_bookmarkCatalog = bookmarkCatalog;
}

platform::HttpClient::Headers CatalogHeadersProvider::GetHeaders()
{
  web_api::HeadersParams params;
  params.m_currentPosition = m_positionProvider.GetCurrentPosition();

  auto const & targetCountry = m_storage.GetLastDownloadedCountryId();
  std::set<base::GeoObjectId> countries;
  auto & cities = params.m_cityGeoIds;
  if (!targetCountry.empty())
  {
    auto const countryIds = m_storage.GetTopCountryGeoIds(targetCountry);
    countries.insert(countryIds.cbegin(), countryIds.cend());
    params.m_countryGeoIds.assign(countries.cbegin(), countries.cend());

    auto const cityIds = m_storage.GetMwmTopCityGeoIds(targetCountry);
    cities.insert(cities.end(), cityIds.cbegin(), cityIds.cend());
  }

  if (m_bookmarkCatalog != nullptr && !m_bookmarkCatalog->GetDownloadedIds().empty())
  {
    auto const & ids = m_bookmarkCatalog->GetDownloadedIds();
    params.m_downloadedGuidesIds.assign(ids.cbegin(), ids.cend());
  }

  return web_api::GetCatalogHeaders(params);
}

std::optional<platform::HttpClient::Header> CatalogHeadersProvider::GetPositionHeader()
{
  if (!m_positionProvider.GetCurrentPosition())
    return {};

  return web_api::GetPositionHeader(*m_positionProvider.GetCurrentPosition());
}
