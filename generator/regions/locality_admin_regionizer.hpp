#pragma once

#include "generator/regions/node.hpp"
#include "generator/regions/place_point.hpp"

namespace generator
{
namespace regions
{
class LocalityAdminRegionizer
{
public:
  LocalityAdminRegionizer(PlacePoint const & placePoint);

  bool ApplyTo(Node::Ptr & tree) const;

protected:
  bool IsFitNode(Node::Ptr & node) const;
  bool HasSearchName(RegionPlace const & place) const;
  void InsertInto(Node::Ptr & node) const;

  bool ContainsSameLocality(Node::Ptr const & tree) const;

private:
  PlacePoint const & m_placePoint;
};
}  // namespace regions
}  // namespace generator
