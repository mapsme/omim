#pragma once

#include <cstdint>
#include <limits>

namespace feature
{
struct FakeFeatureIds
{
  static bool IsEditorCreatedFeature(uint32_t id)
  {
    return id >= kEditorCreatedFeaturesStart && id != kIndexGraphStarterId && id != kTransitGraphId;
  }

  static uint32_t constexpr kIndexGraphStarterId = std::numeric_limits<uint32_t>::max();
  static uint32_t constexpr kTransitGraphId = std::numeric_limits<uint32_t>::max() - 1;
  static uint32_t constexpr kEditorCreatedFeaturesStart =
      std::numeric_limits<uint32_t>::max() - 0xfffff;
};

static_assert(FakeFeatureIds::kIndexGraphStarterId != FakeFeatureIds::kTransitGraphId,
              "Fake feature ids should differ.");
}  // namespace feature
