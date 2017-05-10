#pragma once

#include "geometry/region2d.hpp"
#include "geometry/point2d.hpp"

#include <vector>
#include <string>

namespace osm
{
  /// @return false if borderFile can't be opened
  bool LoadBorders(std::string const & borderFile, std::vector<m2::RegionD> & outBorders);
}
