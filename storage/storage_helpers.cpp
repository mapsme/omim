#include "storage/storage_helpers.hpp"

#include "storage/country_info_getter.hpp"
#include "storage/storage.hpp"

namespace storage
{

bool IsPointCoveredByDownloadedMaps(m2::PointD const & position,
                                    Storage const & storage,
                                    CountryInfoGetter const & countryInfoGetter)
{
  return storage.IsNodeDownloaded(countryInfoGetter.GetRegionCountryId(position));
}

m2::RectD CalcLimitRect(TCountryId countryId,
                        Storage const & storage,
                        CountryInfoGetter const & countryInfoGetter)
{
  TCountriesVec leafList;
  storage.GetAllLeavesInSubtree(countryId, leafList);

  m2::RectD boundBox;
  for (auto const & leaf : leafList)
    boundBox.Add(countryInfoGetter.CalcLimitRectForLeaf(leaf));

  return boundBox;
}

} // namespace storage
