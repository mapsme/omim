#include "base/assert.hpp"
#include "base/string_utils.hpp"

#include "std/algorithm.hpp"
#include "std/cmath.hpp"
#include "std/iomanip.hpp"
#include "std/iterator.hpp"
#include "std/target_os.hpp"

#include <boost/algorithm/string/trim.hpp>

namespace strings
{
bool UniString::IsEqualAscii(char const * s) const
{
  return (size() == strlen(s) && equal(begin(), end(), s));
}

SimpleDelimiter::SimpleDelimiter(char const * delims)
{
  string const s(delims);
  string::const_iterator it = s.begin();
  while (it != s.end())
    m_delims.push_back(utf8::unchecked::next(it));
}

SimpleDelimiter::SimpleDelimiter(char delim)
{
  m_delims.push_back(delim);
}

bool SimpleDelimiter::operator()(UniChar c) const
{
  return find(m_delims.begin(), m_delims.end(), c) != m_delims.end();
}

UniChar LastUniChar(string const & s)
{
  if (s.empty())
    return 0;
  utf8::unchecked::iterator<string::const_iterator> iter(s.end());
  --iter;
  return *iter;
}

namespace
{
template <typename T, typename TResult>
bool IntegerCheck(char const * start, char const * stop, T x, TResult & out)
{
  if (errno != EINVAL && *stop == 0 && start != stop)
  {
    out = static_cast<TResult>(x);
    return static_cast<T>(out) == x;
  }
  errno = 0;
  return false;
}
}  // namespace

bool to_int(char const * start, int & i, int base /*= 10*/)
{
  char * stop;
  errno = 0; // Library functions do not reset it.
  long const v = strtol(start, &stop, base);
  return IntegerCheck(start, stop, v, i);
}

bool to_uint(char const * start, unsigned int & i, int base /*= 10*/)
{
  char * stop;
  errno = 0; // Library functions do not reset it.
  unsigned long const v = strtoul(start, &stop, base);
  return IntegerCheck(start, stop, v, i);
}

bool to_uint64(char const * s, uint64_t & i)
{
  char * stop;
#ifdef OMIM_OS_WINDOWS_NATIVE
  i = _strtoui64(s, &stop, 10);
#else
  i = strtoull(s, &stop, 10);
#endif
  return *stop == 0 && s != stop;
}

bool to_int64(char const * s, int64_t & i)
{
  char * stop;
#ifdef OMIM_OS_WINDOWS_NATIVE
  i = _strtoi64(s, &stop, 10);
#else
  i = strtoll(s, &stop, 10);
#endif
  return *stop == 0 && s != stop;
}

bool to_double(char const * s, double & d)
{
  char * stop;
  d = strtod(s, &stop);
  return *stop == 0 && s != stop && isfinite(d);
}

UniString MakeLowerCase(UniString const & s)
{
  UniString result(s);
  MakeLowerCaseInplace(result);
  return result;
}

void MakeLowerCaseInplace(string & s)
{
  UniString uniStr;
  utf8::unchecked::utf8to32(s.begin(), s.end(), back_inserter(uniStr));
  MakeLowerCaseInplace(uniStr);
  s.clear();
  utf8::unchecked::utf32to8(uniStr.begin(), uniStr.end(), back_inserter(s));
}

string MakeLowerCase(string const & s)
{
  string result(s);
  MakeLowerCaseInplace(result);
  return result;
}

UniString Normalize(UniString const & s)
{
  UniString result(s);
  NormalizeInplace(result);
  return result;
}

void NormalizeDigits(string & utf8)
{
  size_t const n = utf8.size();
  size_t const m = n >= 2 ? n - 2 : 0;

  size_t i = 0;
  while (i < n && utf8[i] != '\xEF')
    ++i;
  size_t j = i;

  // Following invariant holds before/between/after loop iterations below:
  // * utf8[0, i) represents a checked part of the input string.
  // * utf8[0, j) represents a normalized version of the utf8[0, i).
  while (i < m)
  {
    if (utf8[i] == '\xEF' && utf8[i + 1] == '\xBC')
    {
      auto const n = utf8[i + 2];
      if (n >= '\x90' && n <= '\x99')
      {
        utf8[j++] = n - 0x90 + '0';
        i += 3;
      }
      else
      {
        utf8[j++] = utf8[i++];
        utf8[j++] = utf8[i++];
      }
    }
    else
    {
      utf8[j++] = utf8[i++];
    }
  }
  while (i < n)
    utf8[j++] = utf8[i++];
  utf8.resize(j);
}

void NormalizeDigits(UniString & us)
{
  size_t const size = us.size();
  for (size_t i = 0; i < size; ++i)
  {
    UniChar const c = us[i];
    if (c >= 0xFF10 /* '０' */ && c <= 0xFF19 /* '９' */)
      us[i] = c - 0xFF10 + '0';
  }
}

namespace
{
char ascii_to_lower(char in)
{
  char const diff = 'z' - 'Z';
  static_assert(diff == 'a' - 'A', "");
  static_assert(diff > 0, "");

  if (in >= 'A' && in <= 'Z')
    return (in + diff);
  return in;
}
}

void AsciiToLower(string & s) { transform(s.begin(), s.end(), s.begin(), &ascii_to_lower); }
void Trim(string & s) { boost::trim(s); }
void Trim(string & s, char const * anyOf) { boost::trim_if(s, boost::is_any_of(anyOf)); }
bool EqualNoCase(string const & s1, string const & s2)
{
  return MakeLowerCase(s1) == MakeLowerCase(s2);
}

UniString MakeUniString(string const & utf8s)
{
  UniString result;
  utf8::unchecked::utf8to32(utf8s.begin(), utf8s.end(), back_inserter(result));
  return result;
}

string ToUtf8(UniString const & s)
{
  string result;
  utf8::unchecked::utf32to8(s.begin(), s.end(), back_inserter(result));
  return result;
}

bool IsASCIIString(string const & str)
{
  for (size_t i = 0; i < str.size(); ++i)
    if (str[i] & 0x80)
      return false;
  return true;
}

bool IsASCIIDigit(UniChar c) { return c >= '0' && c <= '9'; }
bool IsASCIILatin(UniChar c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool StartsWith(UniString const & s, UniString const & p)
{
  if (p.size() > s.size())
    return false;
  for (size_t i = 0; i < p.size(); ++i)
  {
    if (s[i] != p[i])
      return false;
  }
  return true;
}

bool StartsWith(string const & s1, char const * s2) { return (s1.compare(0, strlen(s2), s2) == 0); }
bool EndsWith(string const & s1, char const * s2)
{
  size_t const n = s1.size();
  size_t const m = strlen(s2);
  if (n < m)
    return false;
  return (s1.compare(n - m, m, s2) == 0);
}

bool EndsWith(string const & s1, string const & s2)
{
  return s1.size() >= s2.size() && s1.compare(s1.size() - s2.size(), s2.size(), s2) == 0;
}

string to_string_dac(double d, int dac)
{
  dac = min(numeric_limits<double>::digits10, dac);

  ostringstream ss;

  if (d < 1. && d > -1.)
  {
    string res;
    if (d >= 0.)
    {
      ss << setprecision(dac + 1) << d + 1;
      res = ss.str();
      res[0] = '0';
    }
    else
    {
      ss << setprecision(dac + 1) << d - 1;
      res = ss.str();
      res[1] = '0';
    }
    return res;
  }

  // Calc digits before comma (log10).
  double fD = fabs(d);
  while (fD >= 1.0 && dac < numeric_limits<double>::digits10)
  {
    fD /= 10.0;
    ++dac;
  }

  ss << setprecision(dac) << d;
  return ss.str();
}

bool IsHTML(string const & utf8)
{
  string::const_iterator it = utf8.begin();
  size_t ltCount = 0;
  size_t gtCount = 0;
  while (it != utf8.end())
  {
    UniChar const c = utf8::unchecked::next(it);
    if (c == '<')
      ++ltCount;
    else if (c == '>')
      ++gtCount;
  }
  return (ltCount > 0 && gtCount > 0);
}

bool AlmostEqual(string const & str1, string const & str2, size_t mismatchedCount)
{
  pair<string::const_iterator, string::const_iterator> mis(str1.begin(), str2.begin());
  auto const str1End = str1.end();
  auto const str2End = str2.end();

  for (size_t i = 0; i <= mismatchedCount; ++i)
  {
    auto const end = mis.first + min(distance(mis.first, str1End), distance(mis.second, str2End));
    mis = mismatch(mis.first, end, mis.second);
    if (mis.first == str1End && mis.second == str2End)
      return true;
    if (mis.first != str1End)
      ++mis.first;
    if (mis.second != str2End)
      ++mis.second;
  }
  return false;
}

void ParseCSVRow(string const & s, char const delimiter, vector<string> & target)
{
  target.clear();
  using It = TokenizeIterator<SimpleDelimiter, string::const_iterator, true>;
  for (It it(s, SimpleDelimiter(delimiter)); it; ++it)
  {
    string column = *it;
    strings::Trim(column);
    target.push_back(move(column));
  }

  // Special case: if the string is empty, return an empty array instead of {""}.
  if (target.size() == 1 && target[0].empty())
    target.clear();
}

}  // namespace strings
