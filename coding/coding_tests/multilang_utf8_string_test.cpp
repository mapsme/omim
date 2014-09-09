#include "../../testing/testing.hpp"

#include "../multilang_utf8_string.hpp"

#include "../../base/string_utils.hpp"

#include "../../defines.hpp"

#include "../../3party/utfcpp/source/utf8.h"


namespace
{
  class InsertTokens
  {
    StringUtf8Multilang::Builder & m_builder;
  public:
    InsertTokens(StringUtf8Multilang::Builder & builder) : m_builder(builder) {}
    void operator() (string const & s)
    {
      int i;
      if (strings::to_int(s, i))
        m_builder.AddIndex(i);
      else
        m_builder.AddString(s);
    }
  };

  class AddToMap
  {
    map<string, string> m_res;

  public:
    bool operator() (int8_t l, string const & s)
    {
      string const lang = StringUtf8Multilang::GetLangByCode(l);
      TEST(!lang.empty(), ());
      TEST(m_res.insert(make_pair(lang, s)).second, ());
      return true;
    }

    void Check(string const & lang, char const * s) const
    {
      map<string, string>::const_iterator i = m_res.find(lang);
      TEST(i != m_res.end(), ());

      size_t const count = strlen(s);
      TEST_EQUAL(i->second.size(), count, (s, i->second));
      TEST(equal(s, s + count, i->second.begin()), (s, i->second));
    }
  };

  class GetStrings : public StringUtf8Multilang::BaseProcessor<AddToMap>
  {
    typedef StringUtf8Multilang::BaseProcessor<AddToMap> BaseT;
  public:
    GetStrings(AddToMap & m) : BaseT(m) {}
    void Index(uint32_t index)
    {
      BaseT::String(strings::to_string(index));
    }
  };

  void AddTestString(string const & lang, string const & s, StringUtf8Multilang::Builder & builder)
  {
    TEST(utf8::is_valid(s.begin(), s.end()), ());

    strings::Tokenize(s, FEATURE_NAME_SPLITTER, InsertTokens(builder.AddLanguage(lang)));
  }
}

UNIT_TEST(MultilangString_Smoke)
{
  char const * sEn = "XXX 666 YYY 777 ZZZ";
  char const * sRu = "14 ыыы яяя 88";
  char const * sBe = "Купалауская";
  char const * sInt = "16000";

  StringUtf8Multilang::Builder builder;
  AddTestString("en", sEn, builder);
  AddTestString("ru", sRu, builder);
  AddTestString("be", sBe, builder);
  AddTestString("int_name", sInt, builder);

  AddToMap theMap;
  {
    StringUtf8Multilang s;
    s.MakeFrom(builder);

    GetStrings doGet(theMap);
    s.ForEachToken(doGet);
  }

  theMap.Check("en", sEn);
  theMap.Check("ru", sRu);
  theMap.Check("be", sBe);
  theMap.Check("int_name", sInt);
}
