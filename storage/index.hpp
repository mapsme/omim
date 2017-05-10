#pragma once

#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace storage
{
using TCountryId = std::string;
using TCountriesSet = std::set<TCountryId>;
using TCountriesVec = std::vector<TCountryId>;

extern const storage::TCountryId kInvalidCountryId;

// @TODO(bykoianko) Check in counrtry tree if the countryId valid.
bool IsCountryIdValid(TCountryId const & countryId);
} //  namespace storage
