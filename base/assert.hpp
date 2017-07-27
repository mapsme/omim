#pragma once
#include "base/base.hpp"
#include "base/internal/message.hpp"
#include "base/src_point.hpp"

#include <cassert>
#include <cstdlib>
#include <string>


namespace my
{
  // Called when ASSERT, CHECK or VERIFY failed.
  // If returns true then crash application.
  typedef bool (*AssertFailedFn)(SrcPoint const &, std::string const &);
  extern AssertFailedFn OnAssertFailed;

  /// @return Pointer to previous message function.
  AssertFailedFn SetAssertFunction(AssertFailedFn fn);
}

#ifdef DEBUG
#define ASSERT_CRASH() assert(false)
#else
#define ASSERT_CRASH() std::abort()
#endif

#define ASSERT_FAIL(msg)                \
  if (::my::OnAssertFailed(SRC(), msg)) \
    ASSERT_CRASH();

// TODO: Evaluate X only once in CHECK().
#define CHECK(X, msg) do { if (X) {} else { \
  ASSERT_FAIL(::my::impl::Message("CHECK("#X")", ::my::impl::Message msg));} } while(false)
#define CHECK_EQUAL(X, Y, msg) do { if ((X) == (Y)) {} else { \
  ASSERT_FAIL(::my::impl::Message("CHECK("#X" == "#Y")", \
                                   ::my::impl::Message(X, Y), \
                                   ::my::impl::Message msg));} } while (false)
#define CHECK_NOT_EQUAL(X, Y, msg) do { if ((X) != (Y)) {} else { \
  ASSERT_FAIL(::my::impl::Message("CHECK("#X" != "#Y")", \
                                   ::my::impl::Message(X, Y), \
                                   ::my::impl::Message msg));} } while (false)
#define CHECK_LESS(X, Y, msg) do { if ((X) < (Y)) {} else { \
  ASSERT_FAIL(::my::impl::Message("CHECK("#X" < "#Y")", \
                                   ::my::impl::Message(X, Y), \
                                   ::my::impl::Message msg));} } while (false)
#define CHECK_LESS_OR_EQUAL(X, Y, msg) do { if ((X) <= (Y)) {} else { \
  ASSERT_FAIL(::my::impl::Message("CHECK("#X" <= "#Y")", \
                                   ::my::impl::Message(X, Y), \
                                   ::my::impl::Message msg));} } while (false)
#define CHECK_GREATER(X, Y, msg) do { if ((X) > (Y)) {} else { \
  ASSERT_FAIL(::my::impl::Message("CHECK("#X" > "#Y")", \
                                   ::my::impl::Message(X, Y), \
                                   ::my::impl::Message msg));} } while (false)
#define CHECK_GREATER_OR_EQUAL(X, Y, msg) do { if ((X) >= (Y)) {} else { \
  ASSERT_FAIL(::my::impl::Message("CHECK("#X" >= "#Y")", \
                                   ::my::impl::Message(X, Y), \
                                   ::my::impl::Message msg));} } while (false)
#define CHECK_OR_CALL(fail, call, X, msg) do { if (X) {} else { \
  if (fail) {\
    ASSERT_FAIL(::my::impl::Message(::my::impl::Message("CHECK("#X")"), \
                                    ::my::impl::Message msg)); \
  } else { \
    call(); \
  } } } while (false)

#ifdef DEBUG
// for Symbian compatibility
#ifdef ASSERT
#undef ASSERT
#endif
#define ASSERT(X, msg) CHECK(X, msg)
#define VERIFY(X, msg) CHECK(X, msg)
#define ASSERT_EQUAL(X, Y, msg) CHECK_EQUAL(X, Y, msg)
#define ASSERT_NOT_EQUAL(X, Y, msg) CHECK_NOT_EQUAL(X, Y, msg)
#define ASSERT_LESS(X, Y, msg) CHECK_LESS(X, Y, msg)
#define ASSERT_LESS_OR_EQUAL(X, Y, msg) CHECK_LESS_OR_EQUAL(X, Y, msg)
#define ASSERT_GREATER(X, Y, msg) CHECK_GREATER(X, Y, msg)
#define ASSERT_GREATER_OR_EQUAL(X, Y, msg) CHECK_GREATER_OR_EQUAL(X, Y, msg)
#else
// for Symbian compatibility
#ifdef ASSERT
#undef ASSERT
#endif
#define ASSERT(X, msg)
#define VERIFY(X, msg) (void)(X)
#define ASSERT_EQUAL(X, Y, msg)
#define ASSERT_NOT_EQUAL(X, Y, msg)
#define ASSERT_LESS(X, Y, msg)
#define ASSERT_LESS_OR_EQUAL(X, Y, msg)
#define ASSERT_GREATER(X, Y, msg)
#define ASSERT_GREATER_OR_EQUAL(X, Y, msg)
#endif
