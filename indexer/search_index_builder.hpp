#pragma once
#include "../std/string.hpp"


namespace indexer
{
  /// @param[in] fName The name of MWM file with extension (without path).
  bool BuildSearchIndexFromDatFile(string const & fName);
}
