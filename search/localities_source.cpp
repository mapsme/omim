#include "search/localities_source.hpp"

#include "indexer/classificator.hpp"

namespace search
{
LocalitiesSource::LocalitiesSource()
{
  auto & c = classif();

//  auto const city = c.GetTypeByPath({"place", "city"});
//  c.ForEachInSubtree([this](uint32_t c) { m_cities.push_back(c); }, city);

//  auto const town = c.GetTypeByPath({"place", "town"});
//  c.ForEachInSubtree([this](uint32_t t) { m_towns.push_back(t); }, town);

  // RELEASE_ONLY: revert LocalitiesSource fix for 8.4 data compatibility.
  m_cities.push_back(c.GetTypeByPath({"place", "city"}));
  m_towns.push_back(c.GetTypeByPath({"place", "town"}));
}
}  // namespace search
