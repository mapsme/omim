#pragma once

#include "base/geo_object_id.hpp"

#include <string>

#include <boost/functional/hash.hpp>

namespace indexer
{
// This struct represents a composite Id.
// This will be useful if we want to distinguish between polygons in a multipolygon.
struct CompositeId
{
  CompositeId() = default;
  explicit CompositeId(std::string const & str);
  explicit CompositeId(base::GeoObjectId mainId, base::GeoObjectId additionalId);
  explicit CompositeId(base::GeoObjectId mainId);

  bool operator<(CompositeId const & other) const;
  bool operator==(CompositeId const & other) const;
  bool operator!=(CompositeId const & other) const;

  std::string ToString() const;

  base::GeoObjectId m_mainId;
  base::GeoObjectId m_additionalId;
};

std::string DebugPrint(CompositeId const & id);
}  // namespace indexer

namespace std
{
template <>
struct hash<indexer::CompositeId>
{
  size_t operator()(indexer::CompositeId const & id) const
  {
    size_t seed = 0;
    boost::hash_combine(seed, std::hash<base::GeoObjectId>()(id.m_mainId));
    boost::hash_combine(seed, std::hash<base::GeoObjectId>()(id.m_additionalId));
    return seed;
  }
};
}  // namespace std
