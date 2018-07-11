#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace osm
{
class Id
{
public:
  static const uint64_t kInvalid = 0ULL;

  explicit Id(uint64_t encodedId = kInvalid);

  static Id Node(uint64_t osmId);
  static Id Way(uint64_t osmId);
  static Id Relation(uint64_t osmId);

  uint64_t OsmId() const;
  uint64_t EncodedId() const;

  bool IsNode() const;
  bool IsWay() const;
  bool IsRelation() const;

  bool operator<(Id const & other) const { return m_encodedId < other.m_encodedId; }
  bool operator==(Id const & other) const { return m_encodedId == other.m_encodedId; }
  bool operator!=(Id const & other) const { return !(*this == other); }
  bool operator==(uint64_t other) const { return OsmId() == other; }

private:
  uint64_t m_encodedId;
};

struct HashId : private std::hash<uint64_t>
{
  size_t operator()(Id const & id) const { return std::hash<uint64_t>::operator()(id.OsmId()); }
};

std::string DebugPrint(osm::Id const & id);
}  // namespace osm
