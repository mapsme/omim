#include "base/base.hpp"
#include "base/assert.hpp"
#include "base/exception.hpp"

#include "std/target_os.hpp"

#include <iostream>

namespace
{
bool g_assertAbortIsEnabled = true;
}

namespace my
{
  void OnAssertFailedDefault(SrcPoint const & srcPoint, std::string const & msg)
  {
    std::cerr << "ASSERT FAILED" << std::endl
              << srcPoint.FileName() << ":" << srcPoint.Line() << std::endl
              << msg << std::endl;
  }

  AssertFailedFn OnAssertFailed = &OnAssertFailedDefault;

  AssertFailedFn SetAssertFunction(AssertFailedFn fn)
  {
    std::swap(OnAssertFailed, fn);
    return fn;
  }

  bool AssertAbortIsEnabled() { return g_assertAbortIsEnabled; }
  void SwitchAssertAbort(bool enable) { g_assertAbortIsEnabled = enable; }
}
