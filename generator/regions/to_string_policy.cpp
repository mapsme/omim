#include "generator/regions/to_string_policy.hpp"

#include "generator/regions/collector_region_info.hpp"

#include "geometry/mercator.hpp"

#include <cstdint>
#include <memory>

#include <boost/optional.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include "3party/jansson/myjansson.hpp"

namespace generator
{
namespace regions
{
std::string JsonPolicy::ToString(NodePath const & path) const
{
  auto geometry = base::NewJSONObject();
  ToJSONObject(*geometry, "type", "Point");
  auto coordinates = base::NewJSONArray();
  auto const & endNode = path.back();
  auto const & endPlace = endNode->GetData();
  auto const tmpCenter = endPlace.GetCenter();
  auto const center = MercatorBounds::ToLatLon({tmpCenter.get<0>(), tmpCenter.get<1>()});
  ToJSONArray(*coordinates, center.lat);
  ToJSONArray(*coordinates, center.lon);
  ToJSONObject(*geometry, "coordinates", coordinates);

  auto localeEn = base::NewJSONObject();
  auto address = base::NewJSONObject();
  for (auto const & p : path)
  {
    auto const & place = p->GetData();
    auto level = place.GetLevel();
    char const * label = GetLabel(level);
    if (!label)
      continue;

    ToJSONObject(*address, label, place.GetName());
    if (m_extendedOutput)
    {
      ToJSONObject(*address, std::string(label) + "_i", DebugPrint(place.GetId()));
      ToJSONObject(*address, std::string(label) + "_a", place.GetRegion().GetArea());
      ToJSONObject(*address, std::string(label) + "_r", static_cast<int>(level));
    }

    ToJSONObject(*localeEn, label, place.GetEnglishOrTransliteratedName());
  }

  auto locales = base::NewJSONObject();
  ToJSONObject(*locales, "en", localeEn);

  auto properties = base::NewJSONObject();
  ToJSONObject(*properties, "name", endPlace.GetName());
  ToJSONObject(*properties, "rank", static_cast<int>(endPlace.GetLevel()));
  ToJSONObject(*properties, "address", address);
  ToJSONObject(*properties, "locales", locales);
  if (path.size() == 1)
  {
    ToJSONObject(*properties, "pid", base::NewJSONNull());
  }
  else
  {
    auto const & parent = (*(path.rbegin() + 1))->GetData();
    ToJSONObject(*properties, "pid", static_cast<int64_t>(parent.GetId().GetEncodedId()));
  }

  auto const & contryRegion = path.front()->GetData().GetRegion();
  if (contryRegion.HasIsoCode())
    ToJSONObject(*properties, "code", contryRegion.GetIsoCode());

  auto feature = base::NewJSONObject();
  ToJSONObject(*feature, "type", "Feature");
  ToJSONObject(*feature, "geometry", geometry);
  ToJSONObject(*feature, "properties", properties);

  auto const cstr = json_dumps(feature.get(), JSON_COMPACT);
  std::unique_ptr<char, JSONFreeDeleter> buffer(cstr);
  return buffer.get();
}
}  // namespace regions
}  // namespace generator
