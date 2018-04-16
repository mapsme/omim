#pragma once
#include "base/assert.hpp"

#include <cstdint>
#include <limits>
#include <type_traits>

namespace bits
{
  // Count the number of 1 bits. Implementation: see Hacker's delight book.
  inline uint32_t PopCount(uint32_t x) noexcept
  {
    x -= ((x >> 1) & 0x55555555);
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    x = (x + (x >> 4)) & 0x0F0F0F0F;
    x += (x >> 8);
    x += (x >> 16);
    return x & 0x3F;
  }

  inline uint32_t PopCount(uint8_t x) noexcept
  {
    return PopCount(static_cast<uint32_t>(x));
  }

  // Count the number of 1 bits in array p, length n bits.
  // There is a better implementation at hackersdelight.org
  inline uint32_t PopCount(uint32_t const * p, uint32_t n)
  {
    uint32_t s = 0;
    for (uint32_t i = 0; i < n; i += 31)
    {
      uint32_t lim = (n < i + 31 ? n : i + 31);
      uint32_t s8 = 0;
      uint32_t x = 0;
      for (uint32_t j = i; j < lim; ++j)
      {
        x = p[j];
        x -= ((x >> 1) & 0x55555555);
        x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
        x = (x + (x >> 4)) & 0x0F0F0F0F;
        s8 += x;
      }
      x = (s8 & 0x00FF00FF) + ((s8 >> 8) & 0x00FF00FF);
      x = (x & 0x0000ffff) + (x >> 16);
      s += x;
    }
    return s;
  }

  static const int SELECT1_ERROR = -1;

  template <typename T> unsigned int select1(T x, unsigned int i)
  {
    // TODO: Fast implementation of select1.
    ASSERT(i > 0 && i <= sizeof(T) * 8, (i));
    for (unsigned int j = 0; j < sizeof(T) * 8; x >>= 1, ++j)
      if (x & 1)
        if (--i == 0)
          return j;
    return static_cast<unsigned int>(SELECT1_ERROR);
  }

  inline uint32_t PopCount(uint64_t x) noexcept
  {
    x = x - ((x & 0xAAAAAAAAAAAAAAAA) >> 1);
    x = (x & 0x3333333333333333) + ((x >> 2) & 0x3333333333333333);
    x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0F;
    x = (x * 0x0101010101010101) >> 56;
    return static_cast<uint32_t>(x);
  }

  inline uint8_t FloorLog(uint64_t x) noexcept
  {
#define CHECK_RSH(x, msb, offset) \
    if (x >> offset)              \
    {                             \
      x >>= offset;               \
      msb += offset;              \
    }

    uint8_t msb = 0;
    CHECK_RSH(x, msb, 32);
    CHECK_RSH(x, msb, 16);
    CHECK_RSH(x, msb, 8);
    CHECK_RSH(x, msb, 4);
    CHECK_RSH(x, msb, 2);
    CHECK_RSH(x, msb, 1);
#undef CHECK_RSH

    return msb;
  }

  // Will be implemented when needed.
  uint64_t PopCount(uint64_t const * p, uint64_t n);

  template <typename T> T RoundLastBitsUpAndShiftRight(T x, T bits)
  {
    return (x & ((1 << bits) - 1)) ? (x >> bits) + 1 : (x >> bits);
  }

  template <typename T> struct LogBitSizeOfType;
  template <> struct LogBitSizeOfType<uint8_t>  { enum { value = 3 }; };
  template <> struct LogBitSizeOfType<uint16_t> { enum { value = 4 }; };
  template <> struct LogBitSizeOfType<uint32_t> { enum { value = 5 }; };
  template <> struct LogBitSizeOfType<uint64_t> { enum { value = 6 }; };

  template <typename T> T ROL(T x)
  {
    return (x << 1) | (x >> (sizeof(T) * 8 - 1));
  }

  template <typename T>
  inline std::make_unsigned_t<T> ZigZagEncode(T x)
  {
    static_assert(std::is_signed<T>::value, "Type should be signed");
    return (x << 1) ^ (x >> (sizeof(x) * 8 - 1));
  }

  template <typename T>
  inline std::make_signed_t<T> ZigZagDecode(T x)
  {
    static_assert(std::is_unsigned<T>::value, "Type should be unsigned.");
    return (x >> 1) ^ -static_cast<std::make_signed_t<T>>(x & 1);
  }

  inline uint32_t PerfectShuffle(uint32_t x)
  {
    x = ((x & 0x0000FF00) << 8) | ((x >> 8) & 0x0000FF00) | (x & 0xFF0000FF);
    x = ((x & 0x00F000F0) << 4) | ((x >> 4) & 0x00F000F0) | (x & 0xF00FF00F);
    x = ((x & 0x0C0C0C0C) << 2) | ((x >> 2) & 0x0C0C0C0C) | (x & 0xC3C3C3C3);
    x = ((x & 0x22222222) << 1) | ((x >> 1) & 0x22222222) | (x & 0x99999999);
    return x;
  }

  inline uint32_t PerfectUnshuffle(uint32_t x)
  {
    x = ((x & 0x22222222) << 1) | ((x >> 1) & 0x22222222) | (x & 0x99999999);
    x = ((x & 0x0C0C0C0C) << 2) | ((x >> 2) & 0x0C0C0C0C) | (x & 0xC3C3C3C3);
    x = ((x & 0x00F000F0) << 4) | ((x >> 4) & 0x00F000F0) | (x & 0xF00FF00F);
    x = ((x & 0x0000FF00) << 8) | ((x >> 8) & 0x0000FF00) | (x & 0xFF0000FF);
    return x;
  }

  // Returns the integer that has the bits of |x| at even-numbered positions
  // and the bits of |y| at odd-numbered positions without changing the
  // relative order of bits coming from |x| and |y|.
  // That is, if the bits of |x| are {x31, x30, ..., x0},
  //         and the bits of |y| are {y31, y30, ..., y0},
  // then the bits of the result are {y31, x31, y30, x30, ..., y0, x0}.
  inline uint64_t BitwiseMerge(uint32_t x, uint32_t y)
  {
    uint32_t const hi = PerfectShuffle((y & 0xFFFF0000) | (x >> 16));
    uint32_t const lo = PerfectShuffle(((y & 0xFFFF) << 16 ) | (x & 0xFFFF));
    return (static_cast<uint64_t>(hi) << 32) + lo;
  }

  inline void BitwiseSplit(uint64_t v, uint32_t & x, uint32_t & y)
  {
    uint32_t const hi = bits::PerfectUnshuffle(static_cast<uint32_t>(v >> 32));
    uint32_t const lo = bits::PerfectUnshuffle(static_cast<uint32_t>(v & 0xFFFFFFFFULL));
    x = ((hi & 0xFFFF) << 16) | (lo & 0xFFFF);
    y =     (hi & 0xFFFF0000) | (lo >> 16);
  }

  // Returns 1 if bit is set and 0 otherwise.
  inline uint8_t GetBit(void const * p, uint32_t offset)
  {
    uint8_t const * pData = static_cast<uint8_t const *>(p);
    return (pData[offset >> 3] >> (offset & 7)) & 1;
  }

  inline void SetBitTo0(void * p, uint32_t offset)
  {
    uint8_t * pData = static_cast<uint8_t *>(p);
    pData[offset >> 3] &= ~(1 << (offset & 7));
  }

  inline void SetBitTo1(void * p, uint32_t offset)
  {
    uint8_t * pData = static_cast<uint8_t *>(p);
    pData[offset >> 3] |= (1 << (offset & 7));
  }
  
  // Compute number of zero bits from the most significant bits side. 
  inline uint32_t NumHiZeroBits32(uint32_t n)
  {
    if (n == 0) return 32;
    uint32_t result = 0;
    while ((n & (uint32_t(1) << 31)) == 0) { ++result; n <<= 1; }
    return result;
  }
  
  inline uint32_t NumHiZeroBits64(uint64_t n)
  {
    if (n == 0) return 64;
    uint32_t result = 0;
    while ((n & (uint64_t(1) << 63)) == 0) { ++result; n <<= 1; }
    return result;
  }
  
  // Computes number of bits needed to store the number, it is not equal to number of ones.
  // E.g. if we have a number (in bit representation) 00001000b then NumUsedBits is 4.
  inline uint32_t NumUsedBits(uint64_t n)
  {
    uint32_t result = 0;
    while (n != 0) { ++result; n >>= 1; }
    return result;
  }

  inline uint64_t GetFullMask(uint8_t numBits)
  {
    ASSERT_LESS_OR_EQUAL(numBits, 64, ());
    return numBits == 64 ? std::numeric_limits<uint64_t>::max()
                         : (static_cast<uint64_t>(1) << numBits) - 1;
  }

  inline bool IsPow2Minus1(uint64_t n) { return (n & (n + 1)) == 0; }
}  // namespace bits
