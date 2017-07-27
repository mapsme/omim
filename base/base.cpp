#include "base/base.hpp"
#include "base/assert.hpp"
#include "base/exception.hpp"

#include "std/target_os.hpp"

#include <iostream>

namespace my
{
  bool OnAssertFailedDefault(SrcPoint const & srcPoint, std::string const & msg)
  {
    std::cerr << "ASSERT FAILED" << std::endl
              << srcPoint.FileName() << ":" << srcPoint.Line() << std::endl
              << msg << std::endl;
    return true;
  }

  AssertFailedFn OnAssertFailed = &OnAssertFailedDefault;

  AssertFailedFn SetAssertFunction(AssertFailedFn fn)
  {
    std::swap(OnAssertFailed, fn);
    return fn;
  }
}
