#include "generator/filter_world.hpp"

#include "search/utils.hpp"

#include "indexer/categories_holder.hpp"
#include "indexer/classificator.hpp"
#include "indexer/scales.hpp"

namespace generator
{
FilterWorld::FilterWorld(std::string const & popularityFilename)
  : m_popularityFilename(popularityFilename)
{
  if (popularityFilename.empty())
    LOG(LWARNING, ("popular_places_data option not set. Popular atractions will not be added to World.mwm"));
}

std::shared_ptr<FilterInterface> FilterWorld::Clone() const
{
  return std::make_shared<FilterWorld>(m_popularityFilename);
}

bool FilterWorld::IsAccepted(feature::FeatureBuilder const & fb)
{
  return IsGoogScale(fb) ||
      IsPopularAttraction(fb, m_popularityFilename) ||
      IsInternationalAirport(fb);
}

// static
bool FilterWorld::IsInternationalAirport(feature::FeatureBuilder const & fb)
{
  auto static const kAirport = classif().GetTypeByPath({"aeroway", "aerodrome", "international"});
  return fb.HasType(kAirport);
}

// static
bool FilterWorld::IsGoogScale(feature::FeatureBuilder const & fb)
{
  // GetMinFeatureDrawScale also checks suitable size for AREA features
  return scales::GetUpperWorldScale() >= fb.GetMinFeatureDrawScale();
}

// static
bool FilterWorld::IsPopularAttraction(feature::FeatureBuilder const & fb, const std::string & popularityFilename)
{
  if (fb.GetName().empty())
    return false;

  auto static const attractionTypes = search::GetCategoryTypes("attractions", "en", GetDefaultCategories());
  ASSERT(is_sorted(attractionTypes.begin(), attractionTypes.end()), ());
  auto const & featureTypes = fb.GetTypes();
  if (!std::any_of(featureTypes.begin(), featureTypes.end(), [](uint32_t t) {
                   return std::binary_search(attractionTypes.begin(), attractionTypes.end(), t);
}))
  {
    return false;
  }

  auto const & m_popularPlaces = PopularPlacesLoader::GetOrLoad(popularityFilename);
  auto const it = m_popularPlaces.find(fb.GetMostGenericOsmId());
  if (it == m_popularPlaces.end())
    return false;

  // todo(@t.yan): adjust
  uint8_t const kPopularityThreshold = 12;
  if (it->second < kPopularityThreshold)
    return false;

  // todo(@t.yan): maybe check place has wikipedia link.
  return true;
}
}  // namespace generator
