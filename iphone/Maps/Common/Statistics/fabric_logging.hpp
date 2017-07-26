#pragma once

#include "base/logging.hpp"

#include <string>

namespace platform
{
  void IosLogMessage(my::LogLevel level, my::SrcPoint const & srcPoint, std::string const & msg);
  void IosAssertMessage(my::SrcPoint const & srcPoint, std::string const & msg);
}
