#include "generator/osm2meta.hpp"

namespace
{
  char HexToDec(char ch)
  {
    if (ch >= '0' && ch <= '9')
      return ch - '0';
    if (ch >= 'a')
      ch -= 'a' - 'A';
    if (ch >= 'A' && ch <= 'F')
      return ch - 'A' + 10;
    return -1;
  }

  string UriDecode(string const & sSrc)
  {
    // This code is based on
    // http://www.codeguru.com/cpp/cpp/string/conversions/article.php/c12759
    //
    // Note from RFC1630: "Sequences which start with a percent
    // sign but are not followed by two hexadecimal characters
    // (0-9, A-F) are reserved for future extension"

    string result(sSrc.length(), 0);
    auto itResult = result.begin();

    for (auto it = sSrc.begin(); it != sSrc.end(); ++it)
    {
      if (*it == '%')
      {
        if (distance(it, sSrc.end()) > 2)
        {
          char dec1 = HexToDec(*(it + 1));
          char dec2 = HexToDec(*(it + 2));
          if (-1 != dec1 && -1 != dec2)
          {
            *itResult++ = (dec1 << 4) + dec2;
            it += 2;
            continue;
          }
        }
      }

      if (*it == '_')
        *itResult++ = ' ';
      else
        *itResult++ = *it;
    }
    
    result.resize(distance(result.begin(), itResult));
    return result;
  }
}  // namespace

string MetadataTagProcessor::ValidateAndFormat_wikipedia(string const & v) const
{
  // Find prefix before ':', shortest case: "lg:aa".
  string::size_type i = v.find(':');
  if (i == string::npos || i < 2 || i + 2 > v.length())
    return string();

  // URL encode lang:title (lang is at most 3 chars), so URL can be reconstructed faster.
  if (i <= 3 || v.substr(0, i) == "be-x-old")
    return v;

  static string::size_type const minUrlPartLength = string("//be.wikipedia.org/wiki/AB").length();
  if (v[i+1] == '/' && i + minUrlPartLength < v.length())
  {
    // Convert URL to "lang:decoded_title".
    i += 3; // skip "://"
    string::size_type const j = v.find('.', i + 1);
    static string const wikiUrlPart = ".wikipedia.org/wiki/";
    if (j != string::npos && v.substr(j, wikiUrlPart.length()) == wikiUrlPart)
      return v.substr(i, j - i) + ":" + UriDecode(v.substr(j + wikiUrlPart.length()));
  }
  return string();
}
