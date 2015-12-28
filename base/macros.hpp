#pragma once

#include "base/assert.hpp"
#include "base/base.hpp"

namespace my
{
    namespace impl
    {
        // http://rsdn.ru/Forum/?mid=1025325
        template <typename T, unsigned int N> char(&ArraySize(T(&)[N]))[N];
    }
}

// Number of elements in array. Compilation error if the type passed is not an array.
#define ARRAY_SIZE(X) sizeof(::my::impl::ArraySize(X))

#define DISALLOW_COPY(className)                             \
private:                                                     \
  className(className const &) = delete;                     \
  className & operator=(className const &) = delete


#define DISALLOW_MOVE(className)                             \
private:                                                     \
  className(className &&) = delete;                          \
  className & operator=(className &&) = delete

#define DISALLOW_COPY_AND_MOVE(className)                    \
  DISALLOW_COPY(className);                                  \
  DISALLOW_MOVE(className)

/////////////////////////////////////////////////////////////

#define TO_STRING_IMPL(x) #x
#define TO_STRING(x) TO_STRING_IMPL(x)

#define UNUSED_VALUE(x) static_cast<void>(x)

namespace my
{
  namespace impl
  {
    template <typename T> inline void ForceUseValue(T const & t)
    {
       volatile T dummy = t;
       UNUSED_VALUE(dummy);
    }
  }
}

// Prevent compiler optimization.
#define FORCE_USE_VALUE(x) ::my::impl::ForceUseValue(x)

#ifdef __GNUC__
#define PREDICT(x, prediction) __builtin_expect(x, prediction)
#define PREDICT_TRUE(x) __builtin_expect((x) != 0, 1)
#define PREDICT_FALSE(x) __builtin_expect((x) != 0, 0)
#else
#define PREDICT(x, prediction) (x)
#define PREDICT_TRUE(x) (x)
#define PREDICT_FALSE(x) (x)
#endif

#define UINT16_FROM_UINT8(hi, lo) ((static_cast<uint16_t>(hi) << 8) | lo)
#define UINT32_FROM_UINT16(hi, lo) ((static_cast<uint32_t>(hi) << 16) | lo)
#define UINT64_FROM_UINT32(hi, lo) ((static_cast<uint64_t>(hi) << 32) | lo)

#define UINT32_FROM_UINT8(u3, u2, u1, u0) \
  UINT32_FROM_UINT16(UINT16_FROM_UINT8(u3, u2), UINT16_FROM_UINT8(u1, u0))
#define UINT64_FROM_UINT8(u7, u6, u5, u4, u3, u2, u1, u0) \
  UINT64_FROM_UINT32(UINT32_FROM_UINT8(u7, u6, u5, u4), UINT32_FROM_UINT8(u3, u2, u1, u0))

#define UINT16_LO(x) (static_cast<uint8_t>(x & 0xFF))
#define UINT16_HI(x) (static_cast<uint8_t>(x >> 8))
#define UINT32_LO(x) (static_cast<uint16_t>(x & 0xFFFF))
#define UINT32_HI(x) (static_cast<uint16_t>(x >> 16))
#define UINT64_LO(x) (static_cast<uint32_t>(x & 0xFFFFFFFF))
#define UINT64_HI(x) (static_cast<uint32_t>(x >> 32))

#define NOTIMPLEMENTED() ASSERT(false, ("Function", __func__, "is not implemented!"));

#if defined(__GNUC__)
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define WARN_UNUSED_RESULT
#endif  // defined(__GNUC__)
