#pragma once

#include "base/geo_object_id.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace generator
{
using PopularityIndex = uint8_t;
using PopularPlaces = std::unordered_map<base::GeoObjectId, PopularityIndex>;

void LoadPopularPlaces(std::string const & srcFilename, PopularPlaces & places);

// |popularity[i]| is i-th feature popularity.
// If there are no popular features in |mwmFile| popularity'll be empty.
void FillPopularityVector(std::string const & srcFilename, std::string const & mwmFile,
                          std::string const & osmToFeatureFilename,
                          std::vector<PopularityIndex> & popularity);

bool BuildPopularPlacesMwmSection(std::string const & srcFilename, std::string const & mwmFile,
                                  std::string const & osmToFeatureFilename);
}  // namespace generator
