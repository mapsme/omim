#pragma once

#include "coding/reader.hpp"
#include "coding/varint.hpp"
#include "coding/writer.hpp"

#include "base/assert.hpp"
#include "base/buffer_vector.hpp"
#include "base/control_flow.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace utils
{
template <class TSink, bool EnableExceptions = false>
void WriteString(TSink & sink, std::string const & s)
{
  if (EnableExceptions && s.empty())
    MYTHROW(Writer::WriteException, ("String is empty"));
  else
    CHECK(!s.empty(), ());

  size_t const sz = s.size();
  WriteVarUint(sink, static_cast<uint32_t>(sz - 1));
  sink.Write(s.c_str(), sz);
}

template <class TSource, bool EnableExceptions = false>
void ReadString(TSource & src, std::string & s)
{
  uint32_t const sz = ReadVarUint<uint32_t>(src) + 1;
  s.resize(sz);
  src.Read(&s[0], sz);

  if (EnableExceptions && s.empty())
    MYTHROW(Reader::ReadException, ("String is empty"));
  else
    CHECK(!s.empty(), ());
}
}  // namespace utils

// A class to store strings in multiple languages.
// May be used e.g. to store several translations of a feature's name.
//
// The coding scheme is as follows:
// * Pairs of the form (|lang|, |s|) are stored. |s| is a string in the UTF-8
//   encoding and |lang| is one of the 64 supported languages (see the list in the cpp file).
//
// * Each pair is represented by a byte encoding the lang followed by the
//   UTF-8 bytes of the string. Then, all such representations are concatenated
//   into a single std::string.
//   The language code is encoded with 6 bits that are prepended with "10", i.e.
//   10xx xxxx. In the UTF-8 encoding that would be a continuation byte, so
//   if you start reading the string and such a byte appears out of nowhere in
//   a place where a continuation byte is not expected you may be sure
//   that the string for the current language has ended and you've reached the
//   string for the next language. Note that this breaks the self-synchronization property.
//
// * The order of the stored strings is not specified. Any language may come first.
class StringUtf8Multilang
{
public:
  struct Lang
  {
    /// OSM language code (e.g. for name:en it's "en" part).
    std::string m_code;
    /// Native language name.
    std::string m_name;
    /// Transliterators to latin ids.
    std::vector<std::string> m_transliteratorsIds;
  };

  struct Position
  {
    size_t m_begin = 0;
    size_t m_length = 0;
  };

  using TranslationPositions = std::map<int8_t, Position>;

  static int8_t constexpr kUnsupportedLanguageCode = -1;
  static int8_t constexpr kDefaultCode = 0;
  static int8_t constexpr kEnglishCode = 1;
  static int8_t constexpr kInternationalCode = 7;
  static int8_t constexpr kAltNameCode = 53;
  static int8_t constexpr kOldNameCode = 55;
  /// How many languages we support on indexing stage. See full list in cpp file.
  /// TODO(AlexZ): Review and replace invalid languages by valid ones.
  static int8_t constexpr kMaxSupportedLanguages = 64;
  // 6 bits language code mask. The language code is encoded with 6 bits that are prepended with
  // "10".
  static int8_t constexpr kLangCodeMask = 0x3F;
  static_assert(kMaxSupportedLanguages == kLangCodeMask + 1, "");
  static char constexpr kReservedLang[] = "reserved";

  using Languages = buffer_vector<Lang, kMaxSupportedLanguages>;

  static Languages const & GetSupportedLanguages();

  /// @returns kUnsupportedLanguageCode if language is not recognized.
  static int8_t GetLangIndex(std::string const & lang);
  /// @returns empty string if langCode is invalid.
  static char const * GetLangByCode(int8_t langCode);
  /// @returns empty string if langCode is invalid.
  static char const * GetLangNameByCode(int8_t langCode);
  /// @returns empty vector if langCode is invalid.
  static std::vector<std::string> const & GetTransliteratorsIdsByCode(int8_t langCode);

  inline bool operator==(StringUtf8Multilang const & rhs) const { return m_s == rhs.m_s; }
  inline bool operator!=(StringUtf8Multilang const & rhs) const { return !(*this == rhs); }

  inline void Clear() { m_s.clear(); }
  inline bool IsEmpty() const { return m_s.empty(); }

  // This method complexity is O(||utf8s||) when adding a new name and O(||m_s|| + ||utf8s||) when
  // replacing an existing name.
  void AddString(int8_t lang, std::string const & utf8s);
  void AddString(std::string const & lang, std::string const & utf8s)
  {
    int8_t const l = GetLangIndex(lang);
    if (l != kUnsupportedLanguageCode)
      AddString(l, utf8s);
  }

  // This method complexity is O(||m_s||).
  void RemoveString(int8_t lang);
  void RemoveString(std::string const & lang)
  {
    int8_t const l = GetLangIndex(lang);
    if (l != kUnsupportedLanguageCode)
      RemoveString(l);
  }

  // Calls |fn| for each pair of |lang| and |utf8s| stored in this multilang string.
  template <typename Fn>
  void ForEach(Fn && fn) const
  {
    size_t i = 0;
    size_t const sz = m_s.size();
    base::ControlFlowWrapper<Fn> wrapper(std::forward<Fn>(fn));
    while (i < sz)
    {
      size_t const next = GetNextIndex(i);
      int8_t const code = m_s[i] & kLangCodeMask;
      if (GetLangByCode(code) != kReservedLang &&
          wrapper(code, m_s.substr(i + 1, next - i - 1)) == base::ControlFlow::Break)
      {
        break;
      }
      i = next;
    }
  }

  /// Used for ordered languages, if you want to do something with priority of that order.
  /// \param languages ordered languages names.
  /// \param fn function or functor, using base::ControlFlow as return value.
  /// \return true if ForEachLanguage was stopped by base::ControlFlow::Break, false otherwise.
  template <typename Fn>
  bool ForEachLanguage(std::vector<std::string> const & languages, Fn && fn) const
  {
    auto const & translationPositions = GenerateTranslationPositions();

    base::ControlFlowWrapper<Fn> wrapper(std::forward<Fn>(fn));
    for (std::string const & language : languages)
    {
      int8_t const languageCode = GetLangIndex(language);
      if (GetLangByCode(languageCode) != kReservedLang)
      {
        auto const & translationPositionsIt = translationPositions.find(languageCode);
        if (translationPositionsIt != translationPositions.end() &&
            wrapper(languageCode, GetTranslation(translationPositionsIt->second)) ==
                base::ControlFlow::Break)
        {
          return true;
        }
      }
    }
    return false;
  };

  bool GetString(int8_t lang, std::string & utf8s) const;
  bool GetString(std::string const & lang, std::string & utf8s) const
  {
    int8_t const l = GetLangIndex(lang);
    if (l >= 0)
      return GetString(l, utf8s);
    else
      return false;
  }

  bool HasString(int8_t lang) const;

  int8_t FindString(std::string const & utf8s) const;
  size_t CountLangs() const;

  template <class TSink>
  void Write(TSink & sink) const
  {
    utils::WriteString(sink, m_s);
  }

  template <class TSource>
  void Read(TSource & src)
  {
    utils::ReadString(src, m_s);
  }

private:
  TranslationPositions GenerateTranslationPositions() const;
  std::string GetTranslation(Position const & position) const;

  size_t GetNextIndex(size_t i) const;

  std::string m_s;
};

std::string DebugPrint(StringUtf8Multilang const & s);
