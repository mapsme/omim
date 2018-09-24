#include "platform/localization.hpp"

#include <algorithm>

#import <Foundation/Foundation.h>
namespace platform
{
std::string GetLocalizedTypeName(std::string const & type)
{
  auto key = "type." + type;
  std::replace(key.begin(), key.end(), '-', '.');

  return [NSLocalizedString(@(key.c_str()), @"") UTF8String];
}
}  // namespace platform
