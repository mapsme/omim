#pragma once

#include <string>

namespace df
{
namespace watch
{

struct IconInfo
{
  std::string m_name;

  IconInfo() = default;
  explicit IconInfo(std::string const & name) : m_name(name) {}
};

}
}
