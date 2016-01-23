#include "editor/osm_utils.hpp"

#include "base/string_utils.hpp"

namespace editor
{
namespace utils
{
ms::LatLon PointFromLatLon(pugi::xml_node const & node)
{
  ms::LatLon ll;
  if (!strings::to_double(node.attribute("lat").value(), ll.lat))
  {
    MYTHROW(editor::utils::NoLatLonError,
            ("Can't parse lat attribute: " + string(node.attribute("lat").value())));
  }
  if (!strings::to_double(node.attribute("lon").value(), ll.lon))
  {
    MYTHROW(editor::utils::NoLatLonError,
            ("Can't parse lon attribute: " + string(node.attribute("lon").value())));
  }
  return ll;
}
}  // namespace utils
}  // namespace editor
