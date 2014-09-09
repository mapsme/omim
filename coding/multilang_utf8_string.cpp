#include "multilang_utf8_string.hpp"
#include "byte_stream.hpp"

#include "../defines.hpp"


static char const * gLangs[] = {
    "default",
    "en", "ja", "fr", "ko_rm", "ar", "de", "int_name", "ru", "sv", "zh", "fi", "be", "ka", "ko",
    "he", "nl", "ga", "ja_rm", "el", "it", "es", "zh_pinyin", "th", "cy", "sr", "uk", "ca", "hu",
    "hsb", "eu", "fa", "br", "pl", "hy", "kn", "sl", "ro", "sq", "am", "fy", "cs", "gd", "sk",
    "af", "ja_kana", "lb", "pt", "hr", "fur", "vi", "tr", "bg", "eo", "lt", "la", "kk", "gsw",
    "et", "ku", "mn", "mk", "lv", "hi" };

int8_t StringUtf8Multilang::GetLangIndex(string const & lang)
{
  STATIC_ASSERT(ARRAY_SIZE(gLangs) == MAX_SUPPORTED_LANGUAGES);

  for (size_t i = 0; i < ARRAY_SIZE(gLangs); ++i)
    if (lang == gLangs[i])
      return static_cast<int8_t>(i);

  return UNSUPPORTED_LANGUAGE_CODE;
}

char const * StringUtf8Multilang::GetLangByCode(int8_t langCode)
{
  if (langCode < 0 || langCode > ARRAY_SIZE(gLangs) - 1)
    return "";
  return gLangs[langCode];
}

void StringUtf8Multilang::GetNextIndex(size_t & i) const
{
  size_t const sz = m_s.size();

  while (i < sz && GetType(m_s[i]) == UTF8)
  {
    if ((m_s[i] & 0x80) == 0)
      i += 1;
    else if ((m_s[i] & 0xFE) == 0xFE)
      i += 7;
    else if ((m_s[i] & 0xFC) == 0xFC)
      i += 6;
    else if ((m_s[i] & 0xF8) == 0xF8)
      i += 5;
    else if ((m_s[i] & 0xF0) == 0xF0)
      i += 4;
    else if ((m_s[i] & 0xE0) == 0xE0)
      i += 3;
    else if ((m_s[i] & 0xC0) == 0xC0)
      i += 2;
  }
}

uint32_t StringUtf8Multilang::ReadOffset(size_t & i) const
{
  char const * p = &m_s[i];

  ArrayByteSource src(p);
  uint32_t const index = ReadVarUint<uint32_t>(src);
  ASSERT_LESS(index, MAX_EXTERNAL_STRINGS, ());

  i += (src.PtrC() - p);
  return index;
}

StringUtf8Multilang::Builder & StringUtf8Multilang::Builder::AddLanguage(int8_t lang)
{
  ASSERT(0 <= lang && lang < MAX_SUPPORTED_LANGUAGES, (lang));
  ASSERT_NOT_EQUAL(m_last, LANG, ());

  m_s.push_back(lang | NOT_UTF8_FIRST);

  m_last = LANG;
  return *this;
}

StringUtf8Multilang::Builder & StringUtf8Multilang::Builder::AddIndex(uint32_t index)
{
  ASSERT_LESS(index, MAX_EXTERNAL_STRINGS, ());
  ASSERT(!m_s.empty(), ());

  m_s.push_back(INDEX_IS_NEXT);

  PushBackByteSink<string> sink(m_s);
  WriteVarUint(sink, index);

  m_last = INDEX;
  return *this;
}

StringUtf8Multilang::Builder & StringUtf8Multilang::Builder::AddString(string const & utf8s)
{
  ASSERT(!utf8s.empty(), ());
  ASSERT(!m_s.empty(), ());

  if (m_last == STRING)
    m_s.push_back(STRING_IS_NEXT);

  m_s.insert(m_s.end(), utf8s.begin(), utf8s.end());

  m_last = STRING;
  return *this;
}

void StringUtf8Multilang::Builder::Clear()
{
  m_s.clear();
  m_last = NONE;
}

void StringUtf8Multilang::Builder::AssignTo(StringUtf8Multilang & s)
{
  s.m_s.swap(m_s);
  Clear();
}

namespace
{
  class DoGetString
  {
    int8_t m_lang;
    string & m_s;
  public:
    DoGetString(int8_t lang, string & s) : m_lang(lang), m_s(s) {}
    bool operator() (int8_t lang, string const & s)
    {
      if (lang == m_lang)
      {
        m_s = s;
        return false;
      }
      return true;
    }
  };
}

bool StringUtf8Multilang::GetString(int8_t lang, string & s) const
{
  DoGetString doGet(lang, s);
  {
    BaseProcessor<DoGetString> processor(doGet);
    ForEachToken(processor);
  }
  return !s.empty();
}

namespace
{

struct Printer
{
  string & m_out;
  Printer(string & out) : m_out(out) {}
  bool operator()(int8_t code, string const & name) const
  {
    m_out += string(StringUtf8Multilang::GetLangByCode(code)) + string(":") + name + " ";
    return true;
  }
};

struct Finder
{
  string const & m_s;
  int8_t m_res;
  Finder(string const & s) : m_s(s), m_res(-1) {}
  bool operator()(int8_t code, string const & name)
  {
    if (name == m_s)
    {
      m_res = code;
      return false;
    }
    return true;
  }
};

} // namespace

int8_t StringUtf8Multilang::FindString(string const & utf8s) const
{
  Finder finder(utf8s);
  {
    BaseProcessor<Finder> processor(finder);
    ForEachToken(processor);
  }
  return finder.m_res;
}

string DebugPrint(StringUtf8Multilang const & s)
{
  string out;
  Printer printer(out);
  {
    StringUtf8Multilang::BaseProcessor<Printer> processor(printer);
    s.ForEachToken(processor);
  }
  return out;
}
