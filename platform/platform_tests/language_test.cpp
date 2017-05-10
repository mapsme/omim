#include "testing/testing.hpp"

#include "platform/preferred_languages.hpp"

#include <string>


UNIT_TEST(LangNormalize_Smoke)
{
  char const * arr1[] = { "en", "en-GB", "zh", "es-SP", "zh-penyn", "en-US", "ru_RU", "es" };
  char const * arr2[] = { "en", "en", "zh", "es", "zh", "en", "ru", "es" };
  static_assert(ARRAY_SIZE(arr1) == ARRAY_SIZE(arr2), "");

  for (size_t i = 0; i < ARRAY_SIZE(arr1); ++i)
    TEST_EQUAL(arr2[i], languages::Normalize(arr1[i]), ());
}

UNIT_TEST(PrefLanguages_Smoke)
{
  std::string s = languages::GetPreferred();
  TEST(!s.empty(), ());
  std::cout << "Preferred langs: " << s << std::endl;

  s = languages::GetCurrentOrig();
  TEST(!s.empty(), ());
  std::cout << "Current original lang: " << s << std::endl;

  s = languages::GetCurrentNorm();
  TEST(!s.empty(), ());
  std::cout << "Current normalized lang: " << s << std::endl;
}
