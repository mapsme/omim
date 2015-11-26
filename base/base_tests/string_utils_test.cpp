#include "testing/testing.hpp"

#include "base/string_utils.hpp"
#include "base/logging.hpp"

#include "std/iomanip.hpp"
#include "std/fstream.hpp"
#include "std/bind.hpp"
#include "std/unordered_map.hpp"


/// internal function in base
namespace strings { UniChar LowerUniChar(UniChar c); }

UNIT_TEST(LowerUniChar)
{
  // Load unicode case folding table.

  // To use Platform class here, we need to add many link stuff into .pro file ...
  //string const fName = GetPlatform().WritablePathForFile("CaseFolding.test");
  string const fName = "../../../omim/data/CaseFolding.test";

  ifstream file(fName.c_str());
  if (!file.good())
  {
    LOG(LWARNING, ("Can't open unicode test file", fName));
    return;
  }

  size_t fCount = 0, cCount = 0;
  typedef unordered_map<strings::UniChar, strings::UniString> mymap;
  mymap m;
  string line;
  while (file.good())
  {
    getline(file, line);
    // strip comments
    size_t const sharp = line.find('#');
    if (sharp != string::npos)
      line.erase(sharp);
    strings::SimpleTokenizer semicolon(line, ";");
    if (!semicolon)
      continue;
    string const capital = *semicolon;
    istringstream stream(capital);
    strings::UniChar uc;
    stream >> hex >> uc;
    ++semicolon;
    string const type = *semicolon;
    if (type == " S" || type == " T")
      continue;
    if (type != " C" && type != " F")
      continue;
    ++semicolon;
    string const outChars = *semicolon;
    strings::UniString us;
    strings::SimpleTokenizer spacer(outChars, " ");
    while (spacer)
    {
      stream.clear();
      stream.str(*spacer);
      strings::UniChar smallCode;
      stream >> hex >> smallCode;
      us.push_back(smallCode);
      ++spacer;
    }
    switch (us.size())
    {
      case 0: continue;
      case 1:
      {
        m[uc] = us;
        ++cCount;
        TEST_EQUAL(strings::LowerUniChar(uc), us[0], ());
        TEST_EQUAL(type, " C", ());
        break;
      }
      default: m[uc] = us; ++fCount; TEST_EQUAL(type, " F", ()); break;
    }
  }
  LOG(LINFO, ("Loaded", cCount, "common foldings and", fCount, "full foldings"));

  // full range unicode table test
  for (strings::UniChar c = 0; c < 0x11000; ++c)
  {
    mymap::iterator found = m.find(c);
    if (found == m.end())
    {
      TEST_EQUAL(c, strings::LowerUniChar(c), ());
    }
    else
    {
      strings::UniString const capitalChar(1, c);
      TEST_EQUAL(strings::MakeLowerCase(capitalChar), found->second, ());
    }
  }
}

UNIT_TEST(MakeLowerCase)
{
  string s;

  s = "THIS_IS_UPPER";
  strings::MakeLowerCaseInplace(s);
  TEST_EQUAL(s, "this_is_upper", ());

  s = "THIS_iS_MiXed";
  strings::MakeLowerCaseInplace(s);
  TEST_EQUAL(s, "this_is_mixed", ());

  s = "this_is_lower";
  strings::MakeLowerCaseInplace(s);
  TEST_EQUAL(s, "this_is_lower", ());

  string const utf8("Hola! 99-\xD0\xA3\xD0\x9F\xD0\xAF\xD0\xA7\xD0\x9A\xD0\x90");
  TEST_EQUAL(strings::MakeLowerCase(utf8),
    "hola! 99-\xD1\x83\xD0\xBF\xD1\x8F\xD1\x87\xD0\xBA\xD0\xB0", ());

  s = "\xc3\x9f"; // es-cet
  strings::MakeLowerCaseInplace(s);
  TEST_EQUAL(s, "ss", ());

  strings::UniChar const arr[] = {0x397, 0x10B4, 'Z'};
  strings::UniChar const carr[] = {0x3b7, 0x2d14, 'z'};
  strings::UniString const us(&arr[0], &arr[0] + ARRAY_SIZE(arr));
  strings::UniString const cus(&carr[0], &carr[0] + ARRAY_SIZE(carr));
  TEST_EQUAL(cus, strings::MakeLowerCase(us), ());
}

UNIT_TEST(EqualNoCase)
{
  TEST(strings::EqualNoCase("HaHaHa", "hahaha"), ());
}

UNIT_TEST(to_double)
{
  string s;
  double d;

  s = "0.123";
  TEST(strings::to_double(s, d), ());
  TEST_ALMOST_EQUAL_ULPS(0.123, d, ());

  s = "1.";
  TEST(strings::to_double(s, d), ());
  TEST_ALMOST_EQUAL_ULPS(1.0, d, ());

  s = "0";
  TEST(strings::to_double(s, d), ());
  TEST_ALMOST_EQUAL_ULPS(0., d, ());

  s = "5.6843418860808e-14";
  TEST(strings::to_double(s, d), ());
  TEST_ALMOST_EQUAL_ULPS(5.6843418860808e-14, d, ());

  s = "-2";
  TEST(strings::to_double(s, d), ());
  TEST_ALMOST_EQUAL_ULPS(-2.0, d, ());

  s = "labuda";
  TEST(!strings::to_double(s, d), ());
}

UNIT_TEST(to_int)
{
  int i;
  string s;

  s = "-2";
  TEST(strings::to_int(s, i), ());
  TEST_EQUAL(-2, i, ());

  s = "0";
  TEST(strings::to_int(s, i), ());
  TEST_EQUAL(0, i, ());

  s = "123456789";
  TEST(strings::to_int(s, i), ());
  TEST_EQUAL(123456789, i, ());

  s = "labuda";
  TEST(!strings::to_int(s, i), ());

  s = "AF";
  TEST(strings::to_int(s, i, 16), ());
  TEST_EQUAL(175, i, ());
}

UNIT_TEST(to_uint64)
{
  uint64_t i;
  string s;

  s = "0";
  TEST(strings::to_uint64(s, i), ());
  TEST_EQUAL(0, i, ());

  s = "123456789101112";
  TEST(strings::to_uint64(s, i), ());
  TEST_EQUAL(123456789101112ULL, i, ());

  s = "labuda";
  TEST(!strings::to_uint64(s, i), ());
}

UNIT_TEST(to_int64)
{
  int64_t i;
  string s;

  s = "-24567";
  TEST(strings::to_int64(s, i), ());
  TEST_EQUAL(-24567, i, ());

  s = "0";
  TEST(strings::to_int64(s, i), ());
  TEST_EQUAL(0, i, ());

  s = "12345678911212";
  TEST(strings::to_int64(s, i), ());
  TEST_EQUAL(12345678911212LL, i, ());

  s = "labuda";
  TEST(!strings::to_int64(s, i), ());
}

UNIT_TEST(to_string)
{
  TEST_EQUAL(strings::to_string(0), "0", ());
  TEST_EQUAL(strings::to_string(-0), "0", ());
  TEST_EQUAL(strings::to_string(1), "1", ());
  TEST_EQUAL(strings::to_string(-1), "-1", ());
  TEST_EQUAL(strings::to_string(1234567890), "1234567890", ());
  TEST_EQUAL(strings::to_string(-987654321), "-987654321", ());
  TEST_EQUAL(strings::to_string(0.56), "0.56", ());
  TEST_EQUAL(strings::to_string(-100.2), "-100.2", ());

  // 6 digits after the comma with rounding - it's a default behavior
  TEST_EQUAL(strings::to_string(-0.66666666), "-0.666667", ());

  TEST_EQUAL(strings::to_string(-1.0E2), "-100", ());
  TEST_EQUAL(strings::to_string(1.0E-2), "0.01", ());

  TEST_EQUAL(strings::to_string(123456789123456789ULL), "123456789123456789", ());
  TEST_EQUAL(strings::to_string(-987654321987654321LL), "-987654321987654321", ());
}

UNIT_TEST(to_string_dac)
{
  TEST_EQUAL(strings::to_string_dac(99.9999, 3), "100", ());
  TEST_EQUAL(strings::to_string_dac(-99.9999, 3), "-100", ());
  TEST_EQUAL(strings::to_string_dac(-10.66666666, 7), "-10.6666667", ());
  TEST_EQUAL(strings::to_string_dac(10001.66666666, 8), "10001.66666666", ());
  TEST_EQUAL(strings::to_string_dac(99999.99999999, 8), "99999.99999999", ());
  TEST_EQUAL(strings::to_string_dac(0.7777, 3), "0.778", ());
  TEST_EQUAL(strings::to_string_dac(-0.333333, 4), "-0.3333", ());
  TEST_EQUAL(strings::to_string_dac(2.33, 2), "2.33", ());

  TEST_EQUAL(strings::to_string_dac(-0.0039, 2), "-0", ());
  TEST_EQUAL(strings::to_string_dac(0.0039, 2), "0", ());
  TEST_EQUAL(strings::to_string_dac(-1.0039, 2), "-1", ());
  TEST_EQUAL(strings::to_string_dac(1.0039, 2), "1", ());

  TEST_EQUAL(strings::to_string_dac(0., 5), "0", ());
  TEST_EQUAL(strings::to_string_dac(0., 0), "0", ());

  TEST_EQUAL(strings::to_string_dac(1.0, 6), "1", ());
  TEST_EQUAL(strings::to_string_dac(0.9, 6), "0.9", ());
  TEST_EQUAL(strings::to_string_dac(-1.0, 30), "-1", ());
  TEST_EQUAL(strings::to_string_dac(-0.99, 30), "-0.99", ());

  TEST_EQUAL(strings::to_string_dac(1.0E30, 6), "1e+30", ());
  TEST_EQUAL(strings::to_string_dac(1.0E-15, 15), "0.000000000000001", ());
  TEST_EQUAL(strings::to_string_dac(1.0 + 1.0E-14, 15), "1.00000000000001", ());
}

struct FunctorTester
{
  size_t & m_index;
  vector<string> const & m_tokens;

  explicit FunctorTester(size_t & counter, vector<string> const & tokens)
    : m_index(counter), m_tokens(tokens) {}
  void operator()(string const & s)
  {
    TEST_EQUAL(s, m_tokens[m_index++], ());
  }
};

void TestIter(string const & str, char const * delims, vector<string> const & tokens)
{
  strings::SimpleTokenizer it(str, delims);
  for (size_t i = 0; i < tokens.size(); ++i)
  {
    TEST_EQUAL(true, it, (str, delims, i));
    TEST_EQUAL(i == tokens.size() - 1, it.IsLast(), ());
    TEST_EQUAL(*it, tokens[i], (str, delims, i));
    ++it;
  }
  TEST_EQUAL(false, it, (str, delims));

  size_t counter = 0;
  FunctorTester f = FunctorTester(counter, tokens);
  strings::Tokenize(str, delims, f);
  TEST_EQUAL(counter, tokens.size(), ());
}

UNIT_TEST(SimpleTokenizer)
{
  vector<string> tokens;
  TestIter("", "", tokens);
  TestIter("", "; ", tokens);
  TestIter("  : ;  , ;", "; :,", tokens);

  {
    char const * s[] = {"hello"};
    tokens.assign(&s[0], &s[0] + ARRAY_SIZE(s));
    TestIter("hello", ";", tokens);
  }

  {
    char const * s[] = {"hello", "world"};
    tokens.assign(&s[0], &s[0] + ARRAY_SIZE(s));
    TestIter(" hello, world!", ", !", tokens);
  }

  {
    char const * s[] = {"\xD9\x80", "\xD8\xA7\xD9\x84\xD9\x85\xD9\x88\xD8\xA7\xD9\x81\xD9\x82",
                       "\xD8\xAC"};
    tokens.assign(&s[0], &s[0] + ARRAY_SIZE(s));
    TestIter("\xD9\x87\xD9\x80 - \xD8\xA7\xD9\x84\xD9\x85\xD9\x88\xD8\xA7\xD9\x81\xD9\x82 \xD9\x87\xD8\xAC",
             " -\xD9\x87", tokens);
  }

  {
    char const * s[] = {"27.535536", "53.884926" , "189"};
    tokens.assign(&s[0], &s[0] + ARRAY_SIZE(s));
    TestIter("27.535536,53.884926,189", ",", tokens);
  }

  {
    char const * s[] = {"1", "2"};
    tokens.assign(&s[0], &s[0] + ARRAY_SIZE(s));
    TestIter("/1/2/", "/", tokens);
  }
}

UNIT_TEST(LastUniChar)
{
  TEST_EQUAL(strings::LastUniChar(""), 0, ());
  TEST_EQUAL(strings::LastUniChar("Hello"), 0x6f, ());
  TEST_EQUAL(strings::LastUniChar(" \xD0\x90"), 0x0410, ());
}

UNIT_TEST(GetUniString)
{
  string const s = "Hello, \xD0\x9C\xD0\xB8\xD0\xBD\xD1\x81\xD0\xBA!";
  strings::SimpleTokenizer iter(s, ", !");
  {
    strings::UniChar const s[] = { 'H', 'e', 'l', 'l', 'o' };
    strings::UniString us(&s[0], &s[0] + ARRAY_SIZE(s));
    TEST_EQUAL(iter.GetUniString(), us, ());
  }
  ++iter;
  {
    strings::UniChar const s[] = { 0x041C, 0x0438, 0x043D, 0x0441, 0x043A };
    strings::UniString us(&s[0], &s[0] + ARRAY_SIZE(s));
    TEST_EQUAL(iter.GetUniString(), us, ());
  }
}

UNIT_TEST(MakeUniString_Smoke)
{
  char const s [] = "Hello!";
  TEST_EQUAL(strings::UniString(&s[0], &s[0] + ARRAY_SIZE(s) - 1), strings::MakeUniString(s), ());
}

UNIT_TEST(Normalize)
{
  strings::UniChar const s[] = { 0x1f101, 'H', 0xfef0, 0xfdfc, 0x2150 };
  strings::UniString us(&s[0], &s[0] + ARRAY_SIZE(s));
  strings::UniChar const r[] = { 0x30, 0x2c, 'H', 0x649, 0x631, 0x6cc, 0x627, 0x644,
                                      0x31, 0x2044, 0x37 };
  strings::UniString result(&r[0], &r[0] + ARRAY_SIZE(r));
  strings::NormalizeInplace(us);
  TEST_EQUAL(us, result, ());
}

UNIT_TEST(Normalize_Special)
{
  {
    string const utf8 = "ąĄćłŁÓŻźŃĘęĆ";
    TEST_EQUAL(strings::ToUtf8(strings::Normalize(strings::MakeUniString(utf8))), "aAclLOZzNEeC", ());
  }

  {
    string const utf8 = "əüöğ";
    TEST_EQUAL(strings::ToUtf8(strings::Normalize(strings::MakeUniString(utf8))), "əuog", ());
  }
}

UNIT_TEST(UniStringToUtf8)
{
  char const utf8Text[] = "У нас исходники хранятся в Utf8!";
  strings::UniString uniS = strings::MakeUniString(utf8Text);
  TEST_EQUAL(string(utf8Text), strings::ToUtf8(uniS), ());
}

UNIT_TEST(StartsWith)
{
  using namespace strings;

  TEST(StartsWith(string(), ""), ());

  string s("xyz");
  TEST(StartsWith(s, ""), ());
  TEST(StartsWith(s, "x"), ());
  TEST(StartsWith(s, "xyz"), ());
  TEST(!StartsWith(s, "xyzabc"), ());
  TEST(!StartsWith(s, "ayz"), ());
  TEST(!StartsWith(s, "axy"), ());
}

UNIT_TEST(EndsWith)
{
  using namespace strings;
  TEST(EndsWith(string(), ""), ());

  string s("xyz");
  TEST(EndsWith(s, ""), ());
  TEST(EndsWith(s, "z"), ());
  TEST(EndsWith(s, "yz"), ());
  TEST(EndsWith(s, "xyz"), ());
  TEST(!EndsWith(s, "abcxyz"), ());
  TEST(!EndsWith(s, "ayz"), ());
  TEST(!EndsWith(s, "axyz"), ());
}

UNIT_TEST(UniString_LessAndEqualsAndNotEquals)
{
  vector<strings::UniString> v;
  v.push_back(strings::MakeUniString(""));
  v.push_back(strings::MakeUniString("Tes"));
  v.push_back(strings::MakeUniString("Test"));
  v.push_back(strings::MakeUniString("TestT"));
  v.push_back(strings::MakeUniString("TestTestTest"));
  v.push_back(strings::MakeUniString("To"));
  v.push_back(strings::MakeUniString("To!"));
  for (size_t i = 0; i < v.size(); ++i)
  {
    TEST(v[i] == v[i], ());
    TEST(!(v[i] < v[i]), ());
    for (size_t j = i + 1; j < v.size(); ++j)
    {
      TEST(v[i] < v[j], ());
      TEST(!(v[j] < v[i]), ());
      TEST(v[i] != v[j], ());
      TEST(v[j] != v[i], ());
    }
  }
}

UNIT_TEST(IsUtf8Test)
{
  TEST(!strings::IsASCIIString("Нет"), ());
  TEST(!strings::IsASCIIString("Классненькие места в Жодино.kml"), ());
  TEST(!strings::IsASCIIString("在Zhodino陰涼處.kml"), ());
  TEST(!strings::IsASCIIString("מקום קריר בZhodino.kml"), ());

  TEST(strings::IsASCIIString("YES"), ());
  TEST(strings::IsASCIIString("Nice places in Zhodino.kml"), ());
}

UNIT_TEST(CountNormLowerSymbols)
{
  char const * strs[] = {
    "æüßs",
    "üßü",
    "İŉẖtestὒ",
    "İŉẖ",
    "İŉẖtestὒ",
    "HelloWorld",
    "üßü",
    "",
    "",
    "Тест на не корректную русскую строку",
    "В ответе пустая строка",
    "Überstraße"
  };

  char const * low_strs[] = {
    "æusss",
    "ussu",
    "i\u0307\u02bcnh\u0331testυ\u0313\u0300",
    "i\u0307\u02bcnh\u0331testυ\u0313\u0300",
    "i\u0307\u02bcnh\u0331",
    "helloworld",
    "usu",
    "",
    "empty",
    "Тест на не корректную строку",
    "",
    "uberstras"
  };

  size_t const results [] = {
    4,
    3,
    8,
    0,
    3,
    10,
    0,
    0,
    0,
    0,
    0,
    9
  };


  size_t const test_count = ARRAY_SIZE(strs);

  for (size_t i = 0; i < test_count; ++i)
  {
    strings::UniString source = strings::MakeUniString(strs[i]);
    strings::UniString result = strings::MakeUniString(low_strs[i]);

    size_t res = strings::CountNormLowerSymbols(source, result);
    TEST_EQUAL(res, results[i], ());
  }
}

UNIT_TEST(IsHTML)
{
  using namespace strings;

  TEST(IsHTML("<a href=\"link\">some link</a>"), ());
  TEST(IsHTML("This is: ---> a <b>broken</b> html"), ());
  TEST(!IsHTML("This is not html"), ());
  TEST(!IsHTML("This is not html < too!"), ());
  TEST(!IsHTML("I am > not html"), ());
}

UNIT_TEST(AlmostEqual)
{
  using namespace strings;

  TEST(AlmostEqual("МКАД, 70-й километр", "МКАД, 79-й километр", 2), ());
  TEST(AlmostEqual("MKAD, 60 km", "MKAD, 59 km", 2), ());
  TEST(AlmostEqual("KAD, 5-y kilometre", "KAD, 7-y kilometre", 1), ());
  TEST(AlmostEqual("", "", 2), ());
  TEST(AlmostEqual("The Vista", "The Vista", 2), ());
  TEST(!AlmostEqual("Glasbrook Road", "ул. Петрова", 2), ());
  TEST(!AlmostEqual("MKAD, 600 km", "MKAD, 599 km", 2), ());
  TEST(!AlmostEqual("MKAD, 45-y kilometre", "MKAD, 46", 2), ());
  TEST(!AlmostEqual("ул. Героев Панфиловцев", "ул. Планерная", 2), ());
}

UNIT_TEST(EditDistance)
{
  auto testEditDistance = [](std::string const & s1, std::string const & s2, uint32_t expected)
  {
    TEST_EQUAL(strings::EditDistance(s1.begin(), s1.end(), s2.begin(), s2.end()), expected, ());
  };

  testEditDistance("", "wwwww", 5);
  testEditDistance("", "", 0);
  testEditDistance("abc", "def", 3);
  testEditDistance("zzzvvv", "zzzvvv", 0);
  testEditDistance("a", "A", 1);
  testEditDistance("bbbbb", "qbbbbb", 1);
  testEditDistance("aaaaaa", "aaabaaa", 1);
  testEditDistance("aaaab", "aaaac", 1);
  testEditDistance("a spaces test", "aspacestest", 2);

  auto testUniStringEditDistance =
      [](std::string const & utf1, std::string const & utf2, uint32_t expected)
  {
    auto s1 = strings::MakeUniString(utf1);
    auto s2 = strings::MakeUniString(utf2);
    TEST_EQUAL(strings::EditDistance(s1.begin(), s1.end(), s2.begin(), s2.end()), expected, ());
  };

  testUniStringEditDistance("ll", "l1", 1);
  testUniStringEditDistance("\u0132ij", "\u0133IJ", 3);
}
