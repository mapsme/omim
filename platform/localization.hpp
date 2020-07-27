#pragma once

#include <string>

namespace platform
{
extern std::string GetLocalizedTypeName(std::string const & type);
extern std::string GetLocalizedBrandName(std::string const & brand);
extern std::string GetLocalizedString(std::string const & key);
extern std::string GetCurrencySymbol(std::string const & currencyCode);
}  // namespace platform
