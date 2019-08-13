#pragma once

#include "base/geo_object_id.hpp"

#include <string>
#include <mutex>
#include <unordered_map>

namespace generator
{
using PopularityIndex = uint8_t;
using PopularPlaces = std::unordered_map<base::GeoObjectId, PopularityIndex>;

void LoadPopularPlaces(std::string const & srcFilename, PopularPlaces & places);

bool BuildPopularPlacesMwmSection(std::string const & srcFilename, std::string const & mwmFile,
                                  std::string const & osmToFeatureFilename);


class PopularPlacesLoader
{
public:
  static PopularPlaces const & GetOrLoad(std::string const & filename);

private:
  static std::mutex m_mutex;
  static std::unordered_map<std::string, PopularPlaces> m_plases;
};
}  // namespace generator
