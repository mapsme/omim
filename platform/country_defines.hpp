#pragma once

#include "std/string.hpp"

/// \brief MapOptions reflects type of map data.
/// \note After mwm update (to small mwms) mwm files contain map and routing sections.
/// They contain information previously contained in map(mwm) and car routing(mwm.routing) files.
/// @TODO When we start using small(single) mwm the enum should be kept.
/// It's necessary for correct working with old big mwm which were downloaded previously.
/// After we stop supporting big mwms at all the enum should be deleted.
enum class MapOptions : uint8_t
{
  Nothing = 0x0,             /*!< No map information is contained. */
  Map = 0x1,                 /*!< Map file contains map (mwm) information. */
  CarRouting = 0x2,          /*!< Map file contains routing information. */
  MapWithCarRouting = 0x3    /*!< Map file contains map (mwm) and routing information. */
};

bool HasOptions(MapOptions mask, MapOptions options);

MapOptions IntersectOptions(MapOptions lhs, MapOptions rhs);

MapOptions SetOptions(MapOptions mask, MapOptions options);

MapOptions UnsetOptions(MapOptions mask, MapOptions options);

MapOptions LeastSignificantOption(MapOptions mask);

string DebugPrint(MapOptions options);
