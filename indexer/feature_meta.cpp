#include "indexer/feature_meta.hpp"

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

// Strip broken multibyte character from the tail.
string StripMultiByte(string const & s)
{
  if (s.length() < 5)
    return s;
  string::size_type pos = s.length() - 5;
  string::size_type next = pos;
  while (next < s.length())
  {
    if ((s[next] & 0xE0) == 0xC0)
      next += 2;
    else if ((s[next] & 0xF0) == 0xE0)
      next += 3;
    else if ((s[next] & 0xF8) == 0xF0)
      next += 4;
    else
      next++;
    if (next <= s.length())
      pos = next;
  }

  return s.substr(0, pos);
}
}  // namespace

namespace feature
{
string Metadata::GetWikiURL() const
{
  string value = this->Get(FMD_WIKIPEDIA);
  string::size_type i = value.find(':');
  if (i == string::npos)
    return string();
  return "https://" + value.substr(0, i) + ".wikipedia.org/wiki/" + value.substr(i + 1);
}

string Metadata::GetWikiTitle() const
{
  string value = this->Get(FMD_WIKIPEDIA);
  return UriDecode(value);
}

void Metadata::SetDescription(string const & s)
{
  this->Set(FMD_DESCRIPTION, s.substr(0, kMaxStringLength));
  this->Set(FMD_DESCRIPTION2, s.length() <= kMaxStringLength ? string() : s.substr(kMaxStringLength, kMaxStringLength));
  this->Set(FMD_DESCRIPTION3, s.length() <= kMaxStringLength * 2 ? string() : s.substr(kMaxStringLength * 2, kMaxStringLength));
  this->Set(FMD_DESCRIPTION4, s.length() <= kMaxStringLength * 3 ? string() : StripMultiByte(s.substr(kMaxStringLength * 3, kMaxStringLength)));
}

string Metadata::GetDescription() const
{
  return this->Get(FMD_DESCRIPTION) + this->Get(FMD_DESCRIPTION2) +
    this->Get(FMD_DESCRIPTION3) + this->Get(FMD_DESCRIPTION4);
}

void Metadata::DropDescription()
{
  this->Drop(FMD_DESCRIPTION);
  this->Drop(FMD_DESCRIPTION2);
  this->Drop(FMD_DESCRIPTION3);
  this->Drop(FMD_DESCRIPTION4);
}
}  // namespace feature
