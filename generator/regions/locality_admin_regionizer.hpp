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

  bool ApplyTo(Node::Ptr & tree);

protected:
  bool IsFitNode(Node::Ptr & node);
  void InsertInto(Node::Ptr & node);

  bool ContainsSameLocality(Node::Ptr const & tree);

private:
  PlacePoint const & m_placePoint;
};
}  // namespace regions
}  // namespace generator
