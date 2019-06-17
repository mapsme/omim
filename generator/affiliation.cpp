#include "generator/affiliation.hpp"

namespace feature
{
CountriesFilesAffiliation::CountriesFilesAffiliation(std::string const & borderPath)
  : m_countries(borders::PackedBorders::GetOrCreate(borderPath))
{
}

std::vector<std::string> CountriesFilesAffiliation::GetAffiliations(FeatureBuilder const & fb) const
{
  std::vector<std::string> countries;
  m_countries->ForEachInRect(fb.GetLimitRect(), [&](auto const & countryPolygons) {
    auto const need = fb.ForAnyGeometryPoint([&](auto const & point) {
      auto const & regions = countryPolygons.m_regions;
      return regions.ForAnyInRect(m2::RectD(point, point), [&](auto const & rgn) {
        return rgn.Contains(point);
      });
    });

    if (need)
      countries.emplace_back(countryPolygons.m_name);
  });

  return countries;
}

bool CountriesFilesAffiliation::HasRegionByName(std::string const & name) const
{
  return m_countries->HasRegionByName(name);
}

OneFileAffiliation::OneFileAffiliation(std::string const & filename)
  : m_filename(filename)
{
}

std::vector<std::string> OneFileAffiliation::GetAffiliations(FeatureBuilder const &) const
{
  return {m_filename};
}
}  // namespace feature
