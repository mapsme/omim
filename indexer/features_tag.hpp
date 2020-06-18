#pragma once

#include "platform/mwm_version.hpp"

#include <string>
#include <vector>

// Features section identifier.
enum class FeaturesTag : uint8_t
{
  // Now it's all features except isolines. In future it's possible to separate these features to
  // several tags.
  Common,
  // Isolines.
  Isolines,
  Count
};

// For different applications (search, routing, rendering etc.) we may not need features from all
// tags. Enumeration mode allows to select subset of tags.
enum class FeaturesEnumerationMode : uint8_t
{
  // In this mode only common features are enumerated.
  Common,
  // In this mode all features are enumerated.
  All
};

// Returns tags for selected enumeration mode.
std::vector<FeaturesTag> const & GetFeaturesTags(FeaturesEnumerationMode mode);

// Following methods return corresponding section name for given tag.
std::string GetFeaturesTag(FeaturesTag tag, version::Format format);
std::string GetFeaturesOffsetsTag(FeaturesTag tag);
std::string GetIndexTag(FeaturesTag tag);
std::string GetGeometryFileTag(FeaturesTag tag);
std::string GetTrianglesFileTag(FeaturesTag tag);

std::string DebugPrint(FeaturesTag tag);
std::string DebugPrint(FeaturesEnumerationMode mode);
