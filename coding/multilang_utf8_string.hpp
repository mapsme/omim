#pragma once

#include "coding/varint.hpp"

#include "base/assert.hpp"

#include "std/algorithm.hpp"
#include "std/map.hpp"
#include "std/string.hpp"


namespace utils
{
template <class TSink> void WriteString(TSink & sink, string const & s)
{
  CHECK(!s.empty(), ());

  size_t const sz = s.size();
  WriteVarUint(sink, static_cast<uint32_t>(sz - 1));
  sink.Write(s.c_str(), sz);
}

template <class TSource> void ReadString(TSource & src, string & s)
{
  uint32_t const sz = ReadVarUint<uint32_t>(src) + 1;
  s.resize(sz);
  src.Read(&s[0], sz);

  CHECK(!s.empty(), ());
}
}  // namespace utils

class StringUtf8Multilang
{
  map<int8_t, string> m_strings;

public:
  static int8_t constexpr kUnsupportedLanguageCode = -1;
  static int8_t constexpr kDefaultCode = 0;
  static int8_t constexpr kEnglishCode = 1;
  static int8_t constexpr kInternationalCode = 7;
  /// How many languages we support on indexing stage. See full list in cpp file.
  /// TODO(AlexZ): Review and replace invalid languages by valid ones.
  static int8_t constexpr kMaxSupportedLanguages = 64;

  struct Lang
  {
    /// OSM language code (e.g. for name:en it's "en" part).
    char const * m_code;
    /// Native language name.
    char const * m_name;
  };
  using Languages = array<Lang, kMaxSupportedLanguages>;

  static Languages const & GetSupportedLanguages();

  /// @returns kUnsupportedLanguageCode if language is not recognized.
  static int8_t GetLangIndex(string const & lang);
  /// @returns empty string if langCode is invalid.
  static char const * GetLangByCode(int8_t langCode);
  /// @returns empty string if langCode is invalid.
  static char const * GetLangNameByCode(int8_t langCode);

  inline bool operator== (StringUtf8Multilang const & rhs) const
  {
    if (m_strings.size() != rhs.m_strings.size())
      return false;

    return equal(m_strings.begin(), m_strings.end(), rhs.m_strings.begin());
  }

  inline bool operator!= (StringUtf8Multilang const & rhs) const
  {
    return !(*this == rhs);
  }
  inline void Clear() { m_strings.clear(); }
  inline bool IsEmpty() const { return m_strings.empty(); }
  void AddString(int8_t lang, string const & utf8s);
  void AddString(string const & lang, string const & utf8s)
  {
    int8_t const l = GetLangIndex(lang);
    if (l >= 0)
      AddString(l, utf8s);
  }

  template <class T>
  void ForEach(T && fn) const
  {
    for (auto const & item : m_strings)
    {
      if (!fn(item.first, item.second))
        return;
    }
  }

  bool GetString(int8_t lang, string & utf8s) const;
  bool GetString(string const & lang, string & utf8s) const
  {
    int8_t const l = GetLangIndex(lang);
    if (l >= 0)
      return GetString(l, utf8s);
    else
      return false;
  }

  int8_t FindString(string const & utf8s) const;

  template <class TSink> void Write(TSink & sink) const
  {
    string str;
    for (auto const & item : m_strings)
    {
      str.push_back(item.first | 0x80);
      str.insert(str.end(), item.second.begin(), item.second.end());
    }

    utils::WriteString(sink, str);
  }

  template <class TSource> void Read(TSource & src)
  {
    m_strings.clear();

    string str;
    utils::ReadString(src, str);

    size_t i = 0;
    size_t const sz = str.size();

    auto getNextIndex = [&str](size_t i) -> size_t {
      ++i;
      size_t const sz = str.size();

      while (i < sz && (str[i] & 0xC0) != 0x80)
      {
        if ((str[i] & 0x80) == 0)
          i += 1;
        else if ((str[i] & 0xFE) == 0xFE)
          i += 7;
        else if ((str[i] & 0xFC) == 0xFC)
          i += 6;
        else if ((str[i] & 0xF8) == 0xF8)
          i += 5;
        else if ((str[i] & 0xF0) == 0xF0)
          i += 4;
        else if ((str[i] & 0xE0) == 0xE0)
          i += 3;
        else if ((str[i] & 0xC0) == 0xC0)
          i += 2;
      }

      return i;
    };

    while (i < sz)
    {
      size_t const next = getNextIndex(i);
      m_strings.emplace((str[i] & 0x3F), str.substr(i + 1, next - i - 1));
      i = next;
    }
  }
};

string DebugPrint(StringUtf8Multilang const & s);
