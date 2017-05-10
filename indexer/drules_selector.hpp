#pragma once

#include "indexer/feature.hpp"

#include <string>
#include <memory>
#include <vector>

namespace drule
{

// Runtime feature style selector absract interface.
class ISelector
{
public:
  virtual ~ISelector() = default;

  // If ISelector.Test returns true then style is applicable for the feature,
  // otherwise, if ISelector.Test returns false, style cannot be applied to the feature.
  virtual bool Test(FeatureType const & ft) const = 0;
};

// Factory method which builds ISelector from a string.
unique_ptr<ISelector> ParseSelector(std::string const & str);

// Factory method which builds composite ISelector from a set of string.
unique_ptr<ISelector> ParseSelector(std::vector<std::string> const & strs);

}  // namespace drule
