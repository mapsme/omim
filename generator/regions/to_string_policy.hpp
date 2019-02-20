#pragma once

#include "generator/regions/node.hpp"

#include <string>
#include <vector>

namespace generator
{
namespace regions
{
class ToStringPolicyInterface
{
public:
  virtual ~ToStringPolicyInterface() = default;

  virtual std::string ToString(std::vector<Node::Ptr> const & path) const = 0;
};

class JsonPolicy : public ToStringPolicyInterface
{
public:
  JsonPolicy(bool extendedOutput = false) : m_extendedOutput(extendedOutput) {}

  std::string ToString(std::vector<Node::Ptr> const & path) const override;

private:
  bool m_extendedOutput;
};
}  // namespace regions
}  // namespace generator
