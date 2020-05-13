#pragma once

#include "platform/mwm_version.hpp"

#include <string>
#include <vector>

enum class FeaturesTag : uint8_t
{
  Common,
  Isolines,
  Count
};

enum class FeaturesEnumerationMode : uint8_t
{
  Common,
  All
};

std::vector<FeaturesTag> const & GetFeaturesTags(FeaturesEnumerationMode mode);

std::string GetFeaturesTag(FeaturesTag tag, version::Format format);
std::string GetFeaturesOffsetsTag(FeaturesTag tag);
std::string GetIndexTag(FeaturesTag tag);
std::string GetGeometryFileTag(FeaturesTag tag);
std::string GetTrianglesFileTag(FeaturesTag tag);

std::string DebugPrint(FeaturesTag tag);
std::string DebugPrint(FeaturesEnumerationMode mode);
