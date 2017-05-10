#pragma once

#include <string>
#include <utility>

enum class MapOptions : uint8_t
{
  Nothing = 0x0,
  Map = 0x1,
  CarRouting = 0x2,
  MapWithCarRouting = 0x3
};

using TMwmCounter = uint32_t;
using TMwmSize = uint64_t;
using TLocalAndRemoteSize = std::pair<TMwmSize, TMwmSize>;

bool HasOptions(MapOptions mask, MapOptions options);

MapOptions IntersectOptions(MapOptions lhs, MapOptions rhs);

MapOptions SetOptions(MapOptions mask, MapOptions options);

MapOptions UnsetOptions(MapOptions mask, MapOptions options);

MapOptions LeastSignificantOption(MapOptions mask);

std::string DebugPrint(MapOptions options);
