#pragma once

#include "coding/hex.hpp"

#include <string>

inline std::string UrlEncode(std::string const & rawUrl)
{
  size_t const count = rawUrl.size();
  std::string result;
  result.reserve(count);

  for (size_t i = 0; i < count; ++i)
  {
    char const c = rawUrl[i];
    if (c < '-' || c == '/' || (c > '9' && c < 'A') || (c > 'Z' && c < '_') ||
        c == '`' || (c > 'z' && c < '~') || c > '~')
    {
      result += '%';
      result += NumToHex(c);
    }
    else
      result += rawUrl[i];
  }

  return result;
}

inline std::string UrlDecode(std::string const & encodedUrl)
{
  size_t const count = encodedUrl.size();
  std::string result;
  result.reserve(count);

  for (size_t i = 0; i < count; ++i)
  {
    if (encodedUrl[i] == '%')
    {
      result += FromHex(encodedUrl.substr(i + 1, 2));
      i += 2;
    }
    else
      result += encodedUrl[i];
  }

  return result;
}
