#pragma once

#include "std/string.hpp"

/// \brief MapOptions is type of map files.
/// \note After small mwm update only Map(mwm) files will be used. They contain information
/// which previously contained in Map(mwm) and CarRouting(mwm.routing) files.
/// @TODO After we stop supporting big mwm at all the structure should be deleted.
enum class MapOptions : uint8_t
{
  Nothing = 0x0,             /*!< No map file. */
  Map = 0x1,                 /*!< Map file with mwm extension. */
  CarRouting = 0x2,          /*!< Map file with mwm.routing extension. */
  MapWithCarRouting = 0x3    /*!< Two map files: one with mwm and another one with mwm.routing extensions. */
};

bool HasOptions(MapOptions mask, MapOptions options);

MapOptions IntersectOptions(MapOptions lhs, MapOptions rhs);

MapOptions SetOptions(MapOptions mask, MapOptions options);

MapOptions UnsetOptions(MapOptions mask, MapOptions options);

MapOptions LeastSignificantOption(MapOptions mask);

string DebugPrint(MapOptions options);
