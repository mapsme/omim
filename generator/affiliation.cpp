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
  std::vector<std::reference_wrapper<borders::CountryPolygons const>> countriesContainer;
  m_countries->ForEachInRect(fb.GetLimitRect(), [&](auto const & countryPolygons) {
    countriesContainer.emplace_back(countryPolygons);
  });

  if (countriesContainer.size() == 1)
  {
    borders::CountryPolygons const & countryPolygons= countriesContainer.front();
    countries.emplace_back(countryPolygons.GetName());
  }
  else if (countriesContainer.size() > 1)
  {
    for (borders::CountryPolygons const & countryPolygons : countriesContainer)
    {
      auto const need = fb.ForAnyGeometryPoint([&](auto const & point) {
        return countryPolygons.Contains(point);
      });

      if (need)
        countries.emplace_back(countryPolygons.GetName());
    }
  }

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

bool OneFileAffiliation::HasRegionByName(std::string const & name) const
{
  return name == m_filename;
}
}  // namespace feature
