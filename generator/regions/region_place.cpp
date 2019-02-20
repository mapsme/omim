#include "generator/regions/region_place.hpp"

#include "coding/transliteration.hpp"

namespace generator
{
namespace regions
{
RegionPlace::RegionPlace(ObjectLevel level, Region const & region, boost::optional<PlaceCenter> placeLabel)
  : m_level{level}, m_region{region}, m_placeLabel{placeLabel}
{ }

void RegionPlace::SetLevel(ObjectLevel level)
{
  m_level = level;
}

base::GeoObjectId RegionPlace::GetId() const
{
  if (m_placeLabel)
    return m_placeLabel->GetId();

  return m_region.GetId();
}

std::string RegionPlace::GetName() const
{
  if (m_placeLabel)
    return m_placeLabel->GetName();

  return m_region.GetName();
}
  
std::string RegionPlace::GetEnglishOrTransliteratedName() const
{
  std::string s = m_placeLabel ? m_placeLabel->GetName(StringUtf8Multilang::kEnglishCode)
                               : m_region.GetName(StringUtf8Multilang::kEnglishCode);
  if (!s.empty())
    return s;

  auto const fn = [&s](int8_t code, std::string const & name) {
    if (code != StringUtf8Multilang::kDefaultCode &&
        Transliteration::Instance().Transliterate(name, code, s))
    {
      return base::ControlFlow::Break;
    }

    return base::ControlFlow::Continue;
  };

  if (m_placeLabel)
    m_placeLabel->GetMultilangName().ForEach(fn);
  else
    m_region.GetMultilangName().ForEach(fn);
  return s;
}

PlaceType RegionPlace::GetPlaceType() const
{
  if (m_placeLabel)
    return m_placeLabel->GetPlaceType();

  return m_region.GetPlaceType();
}

BoostPoint RegionPlace::GetCenter() const
{
  if (m_placeLabel)
    return m_placeLabel->GetCenter();

  return m_region.GetCenter();
}
}  // namespace regions
}  // namespace generator
