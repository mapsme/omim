#pragma once

#include <string>

namespace indexer
{
// Builds the latest version of the search index section and writes it to the mwm file.
// An attempt to rewrite the search index of an old mwm may result in a future crash
// when using search because this function does not update mwm's version. This results
// in version mismatch when trying to read the index.
bool BuildSearchIndexFromDataFile(std::string const & path, std::string const & country,
                                  bool forceRebuild, uint32_t threadsCount);
}  // namespace indexer
