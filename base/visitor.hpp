#pragma once

#include <sstream>
#include <string>

namespace base
{
class DebugPrintVisitor
{
public:
  DebugPrintVisitor(std::string const & name) : m_name(name) {}

  template <typename T>
  void operator()(T const & t, char const * name = nullptr)
  {
    if (!m_empty)
      m_os << ", ";
    m_empty = false;

    if (name)
      m_os << name << ": ";
    m_os << DebugPrint(t);
  }

  std::string ToString() const { return m_name + " [" + m_os.str() + "]"; }

private:
  std::string m_name;
  bool m_empty = true;
  std::ostringstream m_os;
};
}  // namespace base

#define DECLARE_VISITOR_WITH_SUFFIX(suffix, ...)     \
  template <typename Visitor>                        \
  void Visit##suffix(Visitor & visitor)              \
  {                                                  \
    __VA_ARGS__;                                     \
  }                                                  \
  template <typename Visitor>                        \
  void Visit##suffix(Visitor & visitor) const        \
  {                                                  \
    __VA_ARGS__;                                     \
  }

#define DECLARE_VISITOR(...)                         \
  DECLARE_VISITOR_WITH_SUFFIX(, __VA_ARGS__)

#define DECLARE_DEBUG_PRINT(className)               \
  friend std::string DebugPrint(className const & c) \
  {                                                  \
    base::DebugPrintVisitor visitor(#className);     \
    c.Visit(visitor);                                \
    return visitor.ToString();                       \
  }

#define DECLARE_VISITOR_AND_DEBUG_PRINT(className, ...) \
  DECLARE_VISITOR(__VA_ARGS__)                          \
  DECLARE_DEBUG_PRINT(className)
