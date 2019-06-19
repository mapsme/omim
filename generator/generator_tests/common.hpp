#pragma once

#include "generator/osm_element.hpp"

#include <cstdint>
#include <string>

namespace generator_tests
{
using Tags = std::vector<std::pair<std::string, std::string>>;

OsmElement MakeOsmElement(uint64_t id, Tags const & tags, OsmElement::EntityType t);

std::string GetFileName();

bool MakeFakeBordersFile(std::string const & intemediatePath, std::string const & filename);
}  // generator_tests
