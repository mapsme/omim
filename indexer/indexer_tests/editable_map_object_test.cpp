#include "testing/testing.hpp"

#include "indexer/editable_map_object.hpp"

namespace
{
using osm::EditableMapObject;

int8_t GetLangCode(char const * ch)
{
  return StringUtf8Multilang::GetLangIndex(ch);
}
  
UNIT_TEST(EditableMapObject_SetWebsite)
{
  EditableMapObject emo;
  emo.SetWebsite("https://some.thing.org");
  TEST_EQUAL(emo.GetWebsite(), "https://some.thing.org", ());

  emo.SetWebsite("http://some.thing.org");
  TEST_EQUAL(emo.GetWebsite(), "http://some.thing.org", ());

  emo.SetWebsite("some.thing.org");
  TEST_EQUAL(emo.GetWebsite(), "http://some.thing.org", ());

  emo.SetWebsite("");
  TEST_EQUAL(emo.GetWebsite(), "", ());
}

UNIT_TEST(EditableMapObject_ValidateBuildingLevels)
{
  TEST(EditableMapObject::ValidateBuildingLevels(""), ());
  TEST(EditableMapObject::ValidateBuildingLevels("7"), ());
  TEST(EditableMapObject::ValidateBuildingLevels("17"), ());
  TEST(EditableMapObject::ValidateBuildingLevels("25"), ());
  TEST(!EditableMapObject::ValidateBuildingLevels("0"), ());
  TEST(!EditableMapObject::ValidateBuildingLevels("005"), ());
  TEST(!EditableMapObject::ValidateBuildingLevels("26"), ());
  TEST(!EditableMapObject::ValidateBuildingLevels("22a"), ());
  TEST(!EditableMapObject::ValidateBuildingLevels("a22"), ());
  TEST(!EditableMapObject::ValidateBuildingLevels("2a22"), ());
  TEST(!EditableMapObject::ValidateBuildingLevels("ab"), ());
  TEST(!EditableMapObject::ValidateBuildingLevels(
      "2345534564564453645534545345534564564453645"), ());
}

UNIT_TEST(EditableMapObject_ValidateHouseNumber)
{
  TEST(EditableMapObject::ValidateHouseNumber(""), ());
  TEST(EditableMapObject::ValidateHouseNumber("qwer7ty"), ());
  TEST(EditableMapObject::ValidateHouseNumber("12345678"), ());

  // House number must contain at least one number.
  TEST(!EditableMapObject::ValidateHouseNumber("qwerty"), ());
  // House number is too long.
  TEST(!EditableMapObject::ValidateHouseNumber("1234567890123456"), ());
}

UNIT_TEST(EditableMapObject_ValidateFlats)
{
  TEST(EditableMapObject::ValidateFlats(""), ());
  TEST(EditableMapObject::ValidateFlats("123"), ());
  TEST(EditableMapObject::ValidateFlats("123a"), ());
  TEST(EditableMapObject::ValidateFlats("a"), ());
  TEST(EditableMapObject::ValidateFlats("123-456;a-e"), ());
  TEST(EditableMapObject::ValidateFlats("123-456"), ());
  TEST(EditableMapObject::ValidateFlats("123-456; 43-45"), ());
  TEST(!EditableMapObject::ValidateFlats("123-456, 43-45"), ());
  TEST(!EditableMapObject::ValidateFlats("234-234 124"), ());
  TEST(!EditableMapObject::ValidateFlats("123-345-567"), ());
  TEST(!EditableMapObject::ValidateFlats("234-234;234("), ());
  TEST(!EditableMapObject::ValidateFlats("-;"), ());
}

// See search_tests/postcodes_matcher_test.cpp
// UNIT_TEST(EditableMapObject_ValidatePostCode)
// {
// }

UNIT_TEST(EditableMapObject_ValidatePhone)
{
  TEST(EditableMapObject::ValidatePhone(""), ());
  TEST(EditableMapObject::ValidatePhone("+7 000 000 00 00"), ());
  TEST(EditableMapObject::ValidatePhone("+7 (000) 000 00 00"), ());
  TEST(EditableMapObject::ValidatePhone("+7 0000000000"), ());
  TEST(EditableMapObject::ValidatePhone("+7 0000 000 000"), ());
  TEST(EditableMapObject::ValidatePhone("8 0000-000-000"), ());

  TEST(EditableMapObject::ValidatePhone("000 00 00"), ());
  TEST(EditableMapObject::ValidatePhone("000 000 00"), ());
  TEST(EditableMapObject::ValidatePhone("+00 0000 000 000"), ());

  TEST(!EditableMapObject::ValidatePhone("+00 0000 000 0000 000"), ());
  TEST(!EditableMapObject::ValidatePhone("00 00"), ());
  TEST(!EditableMapObject::ValidatePhone("acb"), ());
  TEST(!EditableMapObject::ValidatePhone("000 000 00b"), ());
}

UNIT_TEST(EditableMapObject_ValidateWebsite)
{
  TEST(EditableMapObject::ValidateWebsite(""), ());
  TEST(EditableMapObject::ValidateWebsite("qwe.rty"), ());

  TEST(!EditableMapObject::ValidateWebsite("qwerty"), ());
  TEST(!EditableMapObject::ValidateWebsite(".qwerty"), ());
  TEST(!EditableMapObject::ValidateWebsite("qwerty."), ());
  TEST(!EditableMapObject::ValidateWebsite(".qwerty."), ());
  TEST(!EditableMapObject::ValidateWebsite(".qwerty."), ());
  TEST(!EditableMapObject::ValidateWebsite("w..com"), ());
}

UNIT_TEST(EditableMapObject_ValidateEmail)
{
  TEST(EditableMapObject::ValidateEmail(""), ());
  TEST(EditableMapObject::ValidateEmail("e@ma.il"), ());
  TEST(EditableMapObject::ValidateEmail("e@ma.i.l"), ());
  TEST(EditableMapObject::ValidateEmail("e-m.ail@dot.com.gov"), ());
  TEST(EditableMapObject::ValidateEmail("#$%&'*+-/=?^`_{}|~.@dot.qw.com.gov"), ());

  TEST(!EditableMapObject::ValidateEmail("e.ma.il"), ());
  TEST(!EditableMapObject::ValidateEmail("e@ma@il"), ());
  TEST(!EditableMapObject::ValidateEmail("e@ma@i.l"), ());
  TEST(!EditableMapObject::ValidateEmail("e@mail"), ());
  TEST(!EditableMapObject::ValidateEmail("@email.a"), ());
  TEST(!EditableMapObject::ValidateEmail("emai.l@"), ());
  TEST(!EditableMapObject::ValidateEmail("emai@l."), ());
  TEST(!EditableMapObject::ValidateEmail("e mai@l.com"), ());
  TEST(!EditableMapObject::ValidateEmail("emai@.l"), ());
  TEST(!EditableMapObject::ValidateEmail("emai@_l.ab"), ());
  TEST(!EditableMapObject::ValidateEmail("emai@l_.ab"), ());
  TEST(!EditableMapObject::ValidateEmail("email@e#$%&'*+-/=?^`_{}|~.com"), ());
}

UNIT_TEST(EditableMapObject_CanUseAsDefaultName)
{
  EditableMapObject emo;
  vector<int8_t> const nativeMwmLanguages {GetLangCode("de"), GetLangCode("fr")};

  TEST(EditableMapObject::CanUseAsDefaultName(GetLangCode("de"), nativeMwmLanguages),
       ("Check possibility to use Mwm language code"));
  TEST(EditableMapObject::CanUseAsDefaultName(GetLangCode("fr"), nativeMwmLanguages),
       ("Check possibility to use Mwm language code"));
  TEST(!EditableMapObject::CanUseAsDefaultName(GetLangCode("int_name"), nativeMwmLanguages),
       ("Check possibility to use international language code"));
  TEST(!EditableMapObject::CanUseAsDefaultName(100, nativeMwmLanguages),
       ("Incorrect language code is not supported"));
  TEST(!EditableMapObject::CanUseAsDefaultName(GetLangCode("en"), {GetLangCode("abcd")}),
       ("Incorrect Mwm language name is not supported"));
  TEST(!EditableMapObject::CanUseAsDefaultName(GetLangCode("en"), nativeMwmLanguages),
       ("Can not to use language which not Mwm language or international"));
  TEST(!EditableMapObject::CanUseAsDefaultName(GetLangCode("ru"), nativeMwmLanguages),
       ("Check possibility to use user`s language code"));

  // Trying to use language codes in reverse priority.
  StringUtf8Multilang names;
  names.AddString(GetLangCode("fr"), "second mwm language");
  emo.SetName(names);

  TEST(EditableMapObject::CanUseAsDefaultName(GetLangCode("fr"), nativeMwmLanguages),
       ("It is possible to fix typo"));

  names.AddString(GetLangCode("de"), "first mwm language");
  emo.SetName(names);

  TEST(EditableMapObject::CanUseAsDefaultName(GetLangCode("de"), nativeMwmLanguages),
       ("It is possible to fix typo"));
  TEST(EditableMapObject::CanUseAsDefaultName(GetLangCode("fr"), nativeMwmLanguages),
       ("It is possible to fix typo"));
}

UNIT_TEST(EditableMapObject_GetNamesDataSource)
{
  EditableMapObject emo;
  StringUtf8Multilang names;

  names.AddString(GetLangCode("default"), "Default name");
  names.AddString(GetLangCode("en"), "Eng name");
  names.AddString(GetLangCode("int_name"), "Int name");
  names.AddString(GetLangCode("de"), "De name");
  names.AddString(GetLangCode("ru"), "Ru name");
  names.AddString(GetLangCode("sv"), "Sv name");
  names.AddString(GetLangCode("be"), "By name");
  names.AddString(GetLangCode("ko"), "Ko name");
  names.AddString(GetLangCode("it"), "It name");
  emo.SetName(names);

  vector<int8_t> nativeMwmLanguages = {GetLangCode("de"), GetLangCode("fr")};

  auto const namesDataSource =
      EditableMapObject::GetNamesDataSource(emo.GetName(), nativeMwmLanguages, GetLangCode("ko"));

  TEST_EQUAL(namesDataSource.names.size(), 9, ("All names except the default should be pushed into "
                                           "data source plus empty mandatory names"));
  TEST_EQUAL(namesDataSource.mandatoryNamesCount, 4,
             ("Mandatory names count should be equal == Mwm languages + user`s language"));
  TEST_EQUAL(namesDataSource.names[0].m_code, GetLangCode("de"),
             ("Deutsch name should be first as first language in Mwm"));
  TEST_EQUAL(namesDataSource.names[1].m_code, GetLangCode("fr"),
             ("French name should be second as second language in Mwm"));
  TEST_EQUAL(namesDataSource.names[2].m_code, GetLangCode("en"),
             ("English name should be places after Mwm languages"));
  TEST_EQUAL(namesDataSource.names[3].m_code, GetLangCode("ko"),
             ("Korean name should be fourth because user`s language should be followed by Mwm languages"));

  {
    vector<int8_t> nativeMwmLanguages = {GetLangCode("de"), GetLangCode("fr")};

    auto const namesDataSource =
        EditableMapObject::GetNamesDataSource(emo.GetName(), nativeMwmLanguages, GetLangCode("fr"));
    TEST_EQUAL(namesDataSource.names.size(), 9,
               ("All names + empty mandatory names should be pushed into "
                "data source, except the default one."));
    TEST_EQUAL(namesDataSource.mandatoryNamesCount, 3,
               ("Mandatory names count should be equal == Mwm languages + "
                "english language + user`s language. Excluding repetiton"));
  }
  {
    vector<int8_t> nativeMwmLanguages = {GetLangCode("fr"), GetLangCode("en")};

    auto const namesDataSource =
        EditableMapObject::GetNamesDataSource(emo.GetName(), nativeMwmLanguages, GetLangCode("fr"));
    TEST_EQUAL(namesDataSource.names.size(), 9,
               ("All names + empty mandatory names should be pushed into "
                "data source, except the default one."));
    TEST_EQUAL(namesDataSource.mandatoryNamesCount, 2,
               ("Mandatory names count should be equal == Mwm languages + "
                "english language + user`s language. Excluding repetiton"));
  }
  {
    vector<int8_t> nativeMwmLanguages = {GetLangCode("en"), GetLangCode("en")};

    auto const namesDataSource =
        EditableMapObject::GetNamesDataSource(emo.GetName(), nativeMwmLanguages, GetLangCode("en"));
    TEST_EQUAL(namesDataSource.names.size(), 8,
               ("All names + empty mandatory names should be pushed into "
                "data source, except the default one."));
    TEST_EQUAL(namesDataSource.mandatoryNamesCount, 1,
               ("Mandatory names count should be equal == Mwm languages + "
                "english language + user`s language. Excluding repetiton"));
  }
}
}  // namespace
