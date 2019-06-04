#pragma once

#include "generator/collector_interface.hpp"

#include <sstream>
#include <string>

namespace generator
{
// The class CollectorAddresses is responsible for the collecting addresses to the file.
class CollectorAddresses : public CollectorInterface
{
public:
  CollectorAddresses(std::string const & filename);

  // CollectorInterface overrides:
  void CollectFeature(feature::FeatureBuilder const & feature, OsmElement const &) override;
  void Save() override;

  void Merge(CollectorInterface const * collector) override;
  void MergeInto(CollectorAddresses * collector) const override;

private:
  std::stringstream m_stringStream;
};
}  // namespace generator
