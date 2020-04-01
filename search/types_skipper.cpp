#include "search/types_skipper.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature_data.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "base/stl_helpers.hpp"

#include <algorithm>

using base::StringIL;

namespace search
{
TypesSkipper::TypesSkipper()
{
  Classificator const & c = classif();

  for (auto const & e : (StringIL[]){{"building"}, {"highway"}, {"landuse"}, {"natural"},
                                     {"office"}, {"waterway"}, {"area:highway"}})
  {
    m_skipIfEmptyName[0].push_back(c.GetTypeByPath(e));
  }

  for (auto const & e : (StringIL[]){{"man_made", "chimney"},
                                     {"place", "country"},
                                     {"place", "state"},
                                     {"place", "county"},
                                     {"place", "region"},
                                     {"place", "city"},
                                     {"place", "town"},
                                     {"place", "suburb"},
                                     {"place", "neighbourhood"},
                                     {"place", "square"}})
  {
    m_skipIfEmptyName[1].push_back(c.GetTypeByPath(e));
  }
  m_skipAlways[1].push_back(c.GetTypeByPath({"sponsored", "partner18"}));
  m_skipAlways[1].push_back(c.GetTypeByPath({"sponsored", "partner19"}));
  m_skipAlways[0].push_back(c.GetTypeByPath({"isoline"}));
}

void TypesSkipper::SkipEmptyNameTypes(feature::TypesHolder & types) const
{
  static const TwoLevelPOIChecker dontSkip;
  auto shouldBeRemoved = [this](uint32_t type)
  {
    if (dontSkip.IsMatched(type))
      return false;

    ftype::TruncValue(type, 2);
    if (HasType(m_skipIfEmptyName[1], type))
      return true;

    ftype::TruncValue(type, 1);
    if (HasType(m_skipIfEmptyName[0], type))
      return true;

    return false;
  };

  types.RemoveIf(shouldBeRemoved);
}

bool TypesSkipper::SkipAlways(feature::TypesHolder const & types) const
{
  for (auto type : types)
  {
    ftype::TruncValue(type, 2);
    if (HasType(m_skipAlways[1], type))
      return true;

    ftype::TruncValue(type, 1);
    if (HasType(m_skipAlways[0], type))
      return true;
  }
  return false;
}

// static
bool TypesSkipper::HasType(Cont const & v, uint32_t t)
{
  return std::find(v.begin(), v.end(), t) != v.end();
}
}  // namespace search
