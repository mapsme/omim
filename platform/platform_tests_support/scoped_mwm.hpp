#pragma once

#include "scoped_file.hpp"

#include "base/macros.hpp"

#include <string>

namespace platform
{
namespace tests_support
{
class ScopedFile;

class ScopedMwm
{
public:
  ScopedMwm(std::string const & relativePath);
private:
  ScopedFile m_file;

  DISALLOW_COPY_AND_MOVE(ScopedMwm);
};
}  // namespace tests_support
}  // namespace platform
