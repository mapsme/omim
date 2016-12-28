#include "testing/testing.hpp"

#include "coding/multilang_utf8_string.hpp"

#include "3party/utfcpp/source/utf8.h"


namespace
{
  struct lang_string
  {
    char const * m_lang;
    char const * m_str;
  };

  void TestMultilangString(lang_string const * arr, size_t count)
  {
    StringUtf8Multilang s;

    for (size_t i = 0; i < count; ++i)
    {
      string src(arr[i].m_str);
      TEST(utf8::is_valid(src.begin(), src.end()), ());

      s.AddString(arr[i].m_lang, src);

      string comp;
      TEST(s.GetString(arr[i].m_lang, comp), ());
      TEST_EQUAL(src, comp, ());
    }

    for (size_t i = 0; i < count; ++i)
    {
      string comp;
      TEST(s.GetString(arr[i].m_lang, comp), ());
      TEST_EQUAL(arr[i].m_str, comp, ());
    }

    string test;
    TEST(!s.GetString("xxx", test), ());
  }
}

lang_string gArr[] = { {"default", "default"},
                      {"en", "abcd"},
                      {"ru", "\xD0\xA0\xD0\xB0\xD1\x88\xD0\xBA\xD0\xB0"},
                      {"be", "\xE2\x82\xAC\xF0\xA4\xAD\xA2"} };

UNIT_TEST(MultilangString_Smoke)
{
  StringUtf8Multilang s;

  TestMultilangString(gArr, ARRAY_SIZE(gArr));
}

class LangChecker
{
  size_t m_index;

public:
  LangChecker() : m_index(0) {}
  bool operator() (char lang, string const & utf8s)
  {
    TEST_EQUAL(lang, StringUtf8Multilang::GetLangIndex(gArr[m_index].m_lang), ());
    TEST_EQUAL(utf8s, gArr[m_index].m_str, ());
    ++m_index;
    return true;
  }
};

UNIT_TEST(MultilangString_ForEach)
{
  StringUtf8Multilang s;
  for (size_t i = 0; i < ARRAY_SIZE(gArr); ++i)
    s.AddString(gArr[i].m_lang, gArr[i].m_str);

  s.ForEach(LangChecker());
}

UNIT_TEST(MultilangString_Unique)
{
  StringUtf8Multilang s;
  string cmp;

  s.AddString(0, "xxx");
  TEST(s.GetString(0, cmp), ());
  TEST_EQUAL(cmp, "xxx", ());

  s.AddString(1, "yyy");
  TEST(s.GetString(1, cmp), ());
  TEST_EQUAL(cmp, "yyy", ());

  s.AddString(0, "xxxxxx");
  TEST(s.GetString(0, cmp), ());
  TEST_EQUAL(cmp, "xxxxxx", ());
  TEST(s.GetString(1, cmp), ());
  TEST_EQUAL(cmp, "yyy", ());

  s.AddString(0, "x");
  TEST(s.GetString(0, cmp), ());
  TEST_EQUAL(cmp, "x", ());
  TEST(s.GetString(1, cmp), ());
  TEST_EQUAL(cmp, "yyy", ());
}

UNIT_TEST(MultilangString_LangNames)
{
  TEST_EQUAL("Беларуская", StringUtf8Multilang::GetLangNameByCode(StringUtf8Multilang::GetLangIndex("be")), ());

  auto const & langs = StringUtf8Multilang::GetSupportedLanguages();
  // Using size_t workaround, because our logging/testing macroses do not support passing POD types by value,
  // only by reference. And our constant is a constexpr.
  TEST_EQUAL(langs.size(), size_t(StringUtf8Multilang::kMaxSupportedLanguages), ());
  auto const international = StringUtf8Multilang::GetLangIndex("int_name");
  TEST_EQUAL(langs[international].m_code, "int_name", ());
}

UNIT_TEST(MultilangString_HasString)
{
  StringUtf8Multilang s;
  s.AddString(0, "xxx");
  s.AddString(18, "yyy");
  s.AddString(63, "zzz");
  
  TEST(s.HasString(0), ());
  TEST(s.HasString(18), ());
  TEST(s.HasString(63), ());
  
  TEST(!s.HasString(1), ());
  TEST(!s.HasString(32), ());
}
