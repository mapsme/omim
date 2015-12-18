#pragma once
#include "std/string.hpp"

namespace storage
{
using TIndex = string;

extern const storage::TIndex kInvalidIndex;

bool IsIndexValid(TIndex const & index);
} //  namespace storage
