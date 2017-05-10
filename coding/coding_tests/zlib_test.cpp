#include "testing/testing.hpp"

#include "coding/zlib.hpp"

#include <iterator>
#include <sstream>
#include <string>

using namespace coding;

namespace
{
void TestInflateDeflate(std::string const & original)
{
  std::string compressed;
  TEST(ZLib::Deflate(original, ZLib::Level::BestCompression, std::back_inserter(compressed)), ());

  std::string decompressed;
  TEST(ZLib::Inflate(compressed, std::back_inserter(decompressed)), ());

  TEST_EQUAL(original, decompressed, ());
}

UNIT_TEST(ZLib_Smoke)
{
  {
    std::string s;
    TEST(!ZLib::Deflate(nullptr, 0, ZLib::Level::BestCompression, std::back_inserter(s)), ());
    TEST(!ZLib::Deflate(nullptr, 4, ZLib::Level::BestCompression, std::back_inserter(s)), ());
    TEST(!ZLib::Inflate(nullptr, 0, std::back_inserter(s)), ());
    TEST(!ZLib::Inflate(nullptr, 4, std::back_inserter(s)), ());
  }

  TestInflateDeflate("");
  TestInflateDeflate("Hello, World!");
}

UNIT_TEST(ZLib_Large)
{
  std::string original;
  {
    std::ostringstream os;
    for (size_t i = 0; i < 1000; ++i)
      os << i;
    original = os.str();
  }

  TestInflateDeflate(original);
}
}  // namespace
