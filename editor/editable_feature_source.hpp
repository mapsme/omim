#pragma once

#include "indexer/feature.hpp"
#include "indexer/feature_source.hpp"
#include "indexer/features_tag.hpp"
#include "indexer/mwm_set.hpp"

#include "geometry/rect2d.hpp"

#include <cstdint>
#include <functional>
#include <memory>

class EditableFeatureSource final : public FeatureSource
{
public:
  explicit EditableFeatureSource(MwmSet::MwmHandle const & handle)
    : FeatureSource(handle, FeaturesTag::Common)
  {
  }

  // FeatureSource overrides:
  FeatureStatus GetFeatureStatus(uint32_t index) const override;
  std::unique_ptr<FeatureType> GetModifiedFeature(uint32_t index) const override;
  void ForEachAdditionalFeature(m2::RectD const & rect, int scale,
                                std::function<void(uint32_t)> const & fn) const override;
};

class EditableFeatureSourceFactory : public FeatureSourceFactory
{
public:
  // FeatureSourceFactory overrides:
  std::unique_ptr<FeatureSource> operator()(MwmSet::MwmHandle const & handle,
                                            FeaturesTag tag) const override
  {
    if (tag != FeaturesTag::Common)
      return std::make_unique<FeatureSource>(handle, tag);
    return std::make_unique<EditableFeatureSource>(handle);
  }
};
