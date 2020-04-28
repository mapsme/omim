#pragma once

#include "platform/mwm_version.hpp"

#include <string>

enum class FeaturesTag : uint8_t
{
  Common,
  Count
};

std::string GetFeaturesTag(FeaturesTag tag, version::Format format);
std::string GetFeaturesOffsetsTag(FeaturesTag tag);
std::string GetIndexTag(FeaturesTag tag);
std::string GetGeometryFileTag(FeaturesTag tag);
std::string GetTrianglesFileTag(FeaturesTag tag);

std::string DebugPrint(FeaturesTag tag);
