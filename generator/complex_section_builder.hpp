#pragma once

#include <string>

namespace generator
{
void BuildComplexSection(std::string const & mwmFilename,
                         std::string const & osmToFtIdsFilename,
                         std::string const & complecSrcFilename);
}  // namespace generator
