#pragma once

#include "localads/campaign.hpp"

#include <cstdint>
#include <vector>


namespace localads
{
std::vector<uint8_t> Serialize(std::vector<Campaign> const & campaigns);
std::vector<Campaign> Deserialize(std::vector<uint8_t> const & bytes);
}  // namespace localads
