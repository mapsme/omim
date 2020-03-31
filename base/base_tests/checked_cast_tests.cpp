#include "testing/testing.hpp"

#include "base/checked_cast.hpp"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wimplicitly-unsigned-literal"
UNIT_TEST(IsCastValid)
{
  {
    int8_t value = -1;
    TEST(base::IsCastValid<int8_t>(value), ());
    TEST(base::IsCastValid<int16_t>(value), ());
    TEST(base::IsCastValid<int32_t>(value), ());
    TEST(base::IsCastValid<int64_t>(value), ());

    TEST(!base::IsCastValid<uint8_t>(value), ());
    TEST(!base::IsCastValid<uint16_t>(value), ());
    TEST(!base::IsCastValid<uint32_t>(value), ());
    TEST(!base::IsCastValid<uint64_t>(value), ());
  }
  {
    int64_t value = -1;
    TEST(base::IsCastValid<int8_t>(value), ());
    TEST(base::IsCastValid<int16_t>(value), ());
    TEST(base::IsCastValid<int32_t>(value), ());
    TEST(base::IsCastValid<int64_t>(value), ());

    TEST(!base::IsCastValid<uint8_t>(value), ());
    TEST(!base::IsCastValid<uint16_t>(value), ());
    TEST(!base::IsCastValid<uint32_t>(value), ());
    TEST(!base::IsCastValid<uint64_t>(value), ());
  }
  {
    uint8_t value = 128;
    TEST(!base::IsCastValid<int8_t>(value), ());
    TEST(base::IsCastValid<int16_t>(value), ());
    TEST(base::IsCastValid<int32_t>(value), ());
    TEST(base::IsCastValid<int64_t>(value), ());

    TEST(base::IsCastValid<uint8_t>(value), ());
    TEST(base::IsCastValid<uint16_t>(value), ());
    TEST(base::IsCastValid<uint32_t>(value), ());
    TEST(base::IsCastValid<uint64_t>(value), ());
  }
  {
    uint64_t value = 9223372036854775808;
    TEST(!base::IsCastValid<int8_t>(value), ());
    TEST(!base::IsCastValid<int16_t>(value), ());
    TEST(!base::IsCastValid<int32_t>(value), ());
    TEST(!base::IsCastValid<int64_t>(value), ());

    TEST(!base::IsCastValid<uint8_t>(value), ());
    TEST(!base::IsCastValid<uint16_t>(value), ());
    TEST(!base::IsCastValid<uint32_t>(value), ());
    TEST(base::IsCastValid<uint64_t>(value), ());
  }
  {
    int64_t value = -9223372036854775808;
    TEST(!base::IsCastValid<int8_t>(value), ());
    TEST(!base::IsCastValid<int16_t>(value), ());
    TEST(!base::IsCastValid<int32_t>(value), ());
    TEST(base::IsCastValid<int64_t>(value), ());

    TEST(!base::IsCastValid<uint8_t>(value), ());
    TEST(!base::IsCastValid<uint16_t>(value), ());
    TEST(!base::IsCastValid<uint32_t>(value), ());
    TEST(!base::IsCastValid<uint64_t>(value), ());
  }
}
