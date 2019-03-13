#pragma once

#include "generator/regions/node.hpp"
#include "generator/regions/place_point.hpp"
#include "generator/regions/regions_builder.hpp"

namespace generator
{
namespace regions
{
class AdminSuburbsMarker
{
public:
  void MarkSuburbs(Node::Ptr & tree);

private:
  void MarkSuburbsInCity(Node::Ptr & tree);
  void MarkUnderLocalityAsSublocalities(Node::Ptr & tree);
};
}  // namespace regions
}  // namespace generator

