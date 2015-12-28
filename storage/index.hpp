#pragma once
#include "std/string.hpp"

namespace storage
{
using TIndex = string;

extern const storage::TIndex kInvalidIndex;

// @TODO(bykoianko) Check in counrtry tree if the index valid.
bool IsIndexValid(TIndex const & index);
} //  namespace storage
