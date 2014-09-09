#pragma once

#include "varint.hpp"

#include "../base/assert.hpp"

#include "../std/string.hpp"


namespace utils
{
  template <class TSink> void WriteString(TSink & sink, string const & s)
  {
    CHECK(!s.empty(), ());

    size_t const sz = s.size();
    WriteVarUint(sink, static_cast<uint32_t>(sz-1));
    sink.Write(s.c_str(), sz);
  }

  template <class TSource> void ReadString(TSource & src, string & s)
  {
    uint32_t const sz = ReadVarUint<uint32_t>(src) + 1;
    s.resize(sz);
    src.Read(&s[0], sz);

    CHECK(!s.empty(), ());
  }
}

class StringUtf8Multilang
{
  string m_s;

  void GetNextIndex(size_t & i) const;
  uint32_t ReadOffset(size_t & i) const;

  /// Should be MAX_SUPPORTED_LANGUAGES-1.
  static char const UTF8_GET_LANG_MASK = 0x3F;
  static char const INDEX_IS_NEXT = 0xFE;
  static char const STRING_IS_NEXT = 0xFF;
  static char const NOT_UTF8_FIRST = 0x80;

  enum ByteType { UTF8, LANG, INDEX, STRING };
  inline static ByteType GetType(char c)
  {
    // UTF-8 first byte is always 0xxxxxxx or 11xxxxxx
    // 10xxxxxx is a "language byte"
    if (static_cast<char>(c & 0xC0) == NOT_UTF8_FIRST)
      return LANG;

    switch (c)
    {
    case INDEX_IS_NEXT: return INDEX;
    case STRING_IS_NEXT: return STRING;
    default: return UTF8;
    }
  }

public:
  static int8_t const UNSUPPORTED_LANGUAGE_CODE = -1;
  static int8_t const DEFAULT_CODE = 0;

  /// @return UNSUPPORTED_LANGUAGE_CODE if language is not recognized
  static int8_t GetLangIndex(string const & lang);
  /// @return empty string if langCode is invalid
  static char const * GetLangByCode(int8_t langCode);

  inline bool operator== (StringUtf8Multilang const & rhs) const
  {
    return (m_s == rhs.m_s);
  }

  inline void Clear() { m_s.clear(); }
  inline bool IsEmpty() const { return m_s.empty(); }
  inline void Swap(StringUtf8Multilang & r) { m_s.swap(r.m_s); }

  class Builder
  {
    string m_s;
    enum OperationT { NONE, LANG, STRING, INDEX };
    OperationT m_last;

  public:
    Builder() : m_last(NONE) {}

    Builder & AddLanguage(int8_t lang);
    inline Builder & AddLanguage(string const & lang)
    {
      return AddLanguage(GetLangIndex(lang));
    }
    Builder & AddIndex(uint32_t index);
    Builder & AddString(string const & utf8s);

    inline Builder & AddFullString(int8_t lang, string const & s)
    {
      return AddLanguage(lang).AddString(s);
    }
    inline Builder & AddFullString(string const & lang, string const & s)
    {
      return AddFullString(GetLangIndex(lang), s);
    }

    void Clear();
    void AssignTo(StringUtf8Multilang & s);
  };

  inline void MakeFrom(Builder & b)
  {
    b.AssignTo(*this);
  }

  template <class FnT>
  void ForEachToken(FnT & fn) const
  {
    size_t i = 0;
    size_t const sz = m_s.size();

    while (i < sz)
    {
      size_t next = i;

      switch (GetType(m_s[i]))
      {
      case LANG:
        // check for stop processing
        if (!fn.Language(m_s[i] & UTF8_GET_LANG_MASK))
          return;
        ++next;
        break;

      case INDEX:
        ++next;
        fn.Index(ReadOffset(next));
        break;

      case STRING:
        ++next;
        ++i;
        // break - continue parse string

      case UTF8:
        GetNextIndex(next);
        fn.String(m_s.substr(i, next - i));
        break;
      }

      i = next;
    }
  }

  template <class FnT> class BaseProcessor
  {
    FnT & m_fn;

    string m_str;
    int8_t m_lang;

    bool EmitString()
    {
      bool res = true;
      if (!m_str.empty())
      {
        ASSERT_GREATER_OR_EQUAL(m_lang, 0, ());
        res = m_fn(m_lang, m_str);
        m_str.clear();
      }
      return res;
    }

  public:
    BaseProcessor(FnT & fn) : m_fn(fn) {}
    ~BaseProcessor()
    {
      (void)EmitString();
    }

    bool Language(int8_t lang)
    {
      if (!EmitString())
        return false;

      m_lang = lang;
      return true;
    }

    void Index(uint32_t index)
    {
      ASSERT(false, ());
    }

    void String(string const & s)
    {
      if (!m_str.empty())
        m_str += ' ';
      m_str += s;
    }
  };

  bool GetString(int8_t lang, string & s) const;

  int8_t FindString(string const & utf8s) const;

  template <class TSink> void Write(TSink & sink) const
  {
    utils::WriteString(sink, m_s);
  }

  template <class TSource> void Read(TSource & src)
  {
    utils::ReadString(src, m_s);
  }
};

string DebugPrint(StringUtf8Multilang const & s);
