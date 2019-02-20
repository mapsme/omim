#pragma once

#include "generator/regions/node.hpp"
#include "generator/regions/place_center.hpp"

namespace generator
{
namespace regions
{
class LocalityAdminRegionizer
{
public:
  LocalityAdminRegionizer(PlaceCenter const & placeCenter);

  bool ApplyTo(Node::Ptr & tree);

protected:
  bool IsFitNode(Node::Ptr & node);
  void InsertInto(Node::Ptr & node);

  bool ContainsSameLocality(Node::Ptr const & tree);

private:
  PlaceCenter const & m_placeCenter;
};
}  // namespace regions
}  // namespace generator
