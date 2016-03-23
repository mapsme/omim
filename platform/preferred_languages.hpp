#pragma once

#include "std/string.hpp"

namespace languages
{

/// @note This functions are heavy enough to call them often. Be careful.
//@{

/// @return List of original system languages in the form "en-US|ru-RU|es|zh-Hant".
string GetPreferred();

/// @return Original language code for the current user in the form "en-US", "zh-Hant".
string GetCurrentOrig();

/// @return Normalized language code for the current user in the form "en", "zh".
//@{
/// Returned languages are normalized to our supported languages in the core, see multilang_utf8_string.cpp
/// and should not be used with any sub-locales like zh-Hans/zh-Hant.
/// Some langs like Danish (da) are not supported in the core too, but used as a locale.
string Normalize(string const & lang);
string GetCurrentNorm();
//@}

//@}

}
