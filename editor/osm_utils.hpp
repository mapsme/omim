#pragma once

#include "base/exception.hpp"

#include "geometry/latlon.hpp"

#include "3party/pugixml/src/pugixml.hpp"

namespace editor
{
namespace utils
{
DECLARE_EXCEPTION(NoLatLonError, RootException);

ms::LatLon PointFromLatLon(pugi::xml_node const & node);
}  // namesapce utils
}  // namespace editor
