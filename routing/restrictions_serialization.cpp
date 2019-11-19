#include "routing/restrictions_serialization.hpp"

#include <sstream>

using namespace std;

namespace
{
char const kNo[] = "No";
char const kOnly[] = "Only";
char const kNoUTurn[] = "NoUTurn";
char const kOnlyUTurn[] = "OnlyUTurn";
}  // namespace

namespace routing
{
//static
std::vector<Restriction::Type> const RestrictionHeader::kRestrictionTypes = {
    Restriction::Type::No,
    Restriction::Type::Only,
    Restriction::Type::NoUTurn,
    Restriction::Type::OnlyUTurn
};

// static
uint32_t const Restriction::kInvalidFeatureId = numeric_limits<uint32_t>::max();

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

uint32_t RestrictionHeader::GetNumberOf(Restriction::Type type) const
{
  return m_restrictionCount.at(type);
}

void RestrictionHeader::SetNumberOf(Restriction::Type type, uint32_t size)
{
  m_restrictionCount[type] = size;
}

void RestrictionHeader::Reset()
{
  m_version = kLatestVersion;
  m_reserved = 0;

  for (auto const type : kRestrictionTypes)
    m_restrictionCount.emplace(type, 0);
}

string DebugPrint(Restriction::Type const & type)
{
  switch (type)
  {
  case Restriction::Type::No: return kNo;
  case Restriction::Type::Only: return kOnly;
  case Restriction::Type::NoUTurn: return kNoUTurn;
  case Restriction::Type::OnlyUTurn: return kOnlyUTurn;
  }
  return "Unknown";
}

string DebugPrint(Restriction const & restriction)
{
  ostringstream out;
  out << "[" << DebugPrint(restriction.m_type) << "]: {";
  for (size_t i = 0; i < restriction.m_featureIds.size(); ++i)
  {
    out << restriction.m_featureIds[i];
    if (i + 1 != restriction.m_featureIds.size())
      out << ", ";
  }
  out << "}";
  return out.str();
}

std::string DebugPrint(RestrictionHeader const & header)
{
  std::string res = "RestrictionHeader: { ";
  size_t const n = RestrictionHeader::kRestrictionTypes.size();
  for (size_t i = 0; i < n; ++i)
  {
    auto const type = RestrictionHeader::kRestrictionTypes[i];
    res += DebugPrint(type) + " => " + std::to_string(header.GetNumberOf(type));

    if (i + 1 != n)
      res += ", ";
  }

  res += " }";
  return res;
}

bool IsUTurnType(Restriction::Type type)
{
  return type == Restriction::Type::NoUTurn || type == Restriction::Type::OnlyUTurn;
}
}  // namespace routing
