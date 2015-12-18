#pragma once
#include "std/string.hpp"

namespace storage
{
using TIndex = string;

static const TIndex kInvalidIndex = string("");
bool IsIndexValid(TIndex const & index);
} //  namespace storage
