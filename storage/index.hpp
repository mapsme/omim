#pragma once
#include "std/string.hpp"

namespace storage
{
using TCountryId = string;

extern const storage::TCountryId kInvalidCountryId;

// @TODO(bykoianko) Check in counrtry tree if the countryId valid.
bool IsCountryIdValid(TCountryId const & countryId);
} //  namespace storage
