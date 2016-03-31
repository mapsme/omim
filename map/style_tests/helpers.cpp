#include "helpers.hpp"

#include "indexer/classificator_loader.hpp"
#include "indexer/map_style_reader.hpp"

#include "base/logging.hpp"

namespace styles
{

void RunForEveryMapStyle(function<void(MapStyle)> const & fn)
{
  auto & reader = GetStyleReader();
  for (size_t s = 0; s < MapStyleCount; ++s)
  {
    MapStyle const mapStyle = static_cast<MapStyle>(s);
    if (mapStyle != MapStyle::MapStyleMerged)
    {
      reader.SetCurrentStyle(mapStyle);
      classificator::Load();
      LOG(LINFO, ("Test with map style", mapStyle));
      fn(mapStyle);
    }
  }

  // Restore default style.
  reader.SetCurrentStyle(kDefaultMapStyle);
  classificator::Load();
}

} // namesapce styles
