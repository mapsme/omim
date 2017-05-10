#include "routing/restrictions_serialization.hpp"

#include <sstream>

namespace
{
char const kNo[] = "No";
char const kOnly[] = "Only";
}  // namespace

namespace routing
{
// static
uint32_t const Restriction::kInvalidFeatureId = std::numeric_limits<uint32_t>::max();

bool Restriction::IsValid() const
{
  return !m_featureIds.empty() &&
         std::find(std::begin(m_featureIds), std::end(m_featureIds),
                   kInvalidFeatureId) == std::end(m_featureIds);
}

bool Restriction::operator==(Restriction const & restriction) const
{
  return m_featureIds == restriction.m_featureIds && m_type == restriction.m_type;
}

bool Restriction::operator<(Restriction const & restriction) const
{
  if (m_type != restriction.m_type)
    return m_type < restriction.m_type;
  return m_featureIds < restriction.m_featureIds;
}

std::string ToString(Restriction::Type const & type)
{
  switch (type)
  {
  case Restriction::Type::No: return kNo;
  case Restriction::Type::Only: return kOnly;
  }
  return "Unknown";
}

std::string DebugPrint(Restriction::Type const & type) { return ToString(type); }

std::string DebugPrint(Restriction const & restriction)
{
  std::ostringstream out;
  out << "m_featureIds:[" << ::DebugPrint(restriction.m_featureIds)
      << "] m_type:" << DebugPrint(restriction.m_type) << " ";
  return out.str();
}
}  // namespace routing
