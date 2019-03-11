#include "generator/restriction_writer.hpp"

#include "generator/intermediate_elements.hpp"
#include "generator/restriction_collector.hpp"

#include "routing/restrictions_serialization.hpp"

#include "base/geo_object_id.hpp"
#include "base/logging.hpp"

#include <algorithm>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
using namespace routing;

std::vector<std::pair<std::string, Restriction::Type>> const kRestrictionTypes =
  {{"no_right_turn", Restriction::Type::No},
   {"no_left_turn", Restriction::Type::No},
   {"no_u_turn", Restriction::Type::No},
   {"no_straight_on", Restriction::Type::No},
   {"no_entry", Restriction::Type::No},
   {"no_exit", Restriction::Type::No},
   {"only_right_turn", Restriction::Type::Only},
   {"only_left_turn", Restriction::Type::Only},
   {"only_straight_on", Restriction::Type::Only}};

/// \brief Converts restriction type form string to RestrictionCollector::Type.
/// \returns true if conversion was successful and false otherwise.
bool TagToType(std::string const & tag, Restriction::Type & type)
{
  auto const it = std::find_if(kRestrictionTypes.cbegin(), kRestrictionTypes.cend(),
                          [&tag](std::pair<std::string, Restriction::Type> const & v) {
    return v.first == tag;
  });
  if (it == kRestrictionTypes.cend())
    return false; // Unsupported restriction type.

  type = it->second;
  return true;
}
}  // namespace

namespace routing
{
void RestrictionWriter::Open(std::string const & fullPath)
{
  LOG(LINFO, ("Saving road restrictions in osm id terms to", fullPath));
  m_stream.open(fullPath, std::ofstream::out);

  if (!IsOpened())
    LOG(LINFO, ("Cannot open file", fullPath));
}

void RestrictionWriter::Write(RelationElement const & relationElement)
{
  if (!IsOpened())
  {
    LOG(LWARNING, ("Tried to write to a closed restrictions writer"));
    return;
  }

  CHECK_EQUAL(relationElement.GetType(), "restriction", ());

//  // Note. For the time being only line-point-line road restriction is supported.
//  if (relationElement.nodes.size() != 1 || relationElement.ways.size() != 2)
//    return;  // Unsupported restriction. For example line-line-line.

  auto const getMembersByTag = [&relationElement](std::string const & tag)
  {
    std::vector<RelationElement::Member> result;
    for (auto const & member : relationElement.ways)
    {
      if (member.second == tag)
        result.emplace_back(member);
    }

    for (auto const & member : relationElement.nodes)
    {
      if (member.second == tag)
        result.emplace_back(member);
    }

    return result;
  };

  auto const from = getMembersByTag("from");
  auto const to = getMembersByTag("to");
  auto const via = getMembersByTag("to");

  if (from.size() != 1 || to.size() != 1 || via.empty())
    return;

  // Either 1 node as vis, either several ways as via.
  // https://wiki.openstreetmap.org/wiki/Relation:restriction#Members
  if (via.size() != 1)
  {
    bool allMembersAreWays = std::all_of(via.begin(), via.end(),
                                         [](auto const & member)
                                         {
                                           return member.second == "way";
                                         });
    if (!allMembersAreWays)
      return;
  }

  // Extracting type of restriction.
  auto const tagIt = relationElement.tags.find("restriction");
  if (tagIt == relationElement.tags.end())
    return;

  Restriction::Type type = Restriction::Type::No;
  if (!TagToType(tagIt->second, type))
    return;

  // Adding restriction.
  m_stream << ToString(type) << "," << from.back().first << ", ";
  if (via.back().second != "node")
  {
    for (auto const & viaMember : via)
      m_stream << viaMember.first << ", ";
  }

  m_stream << to.back().first << '\n';
}

bool RestrictionWriter::IsOpened() const { return m_stream && m_stream.is_open(); }
}  // namespace routing
