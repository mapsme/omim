#include "search/mwm_context.hpp"

#include "indexer/cell_id.hpp"
#include "indexer/fake_feature_ids.hpp"
#include "indexer/feature_source.hpp"
#include "indexer/features_tag.hpp"

namespace search
{
void CoverRect(m2::RectD const & rect, int scale, covering::Intervals & result)
{
  covering::CoveringGetter covering(rect, covering::ViewportWithLowLevels);
  auto const & intervals = covering.Get<RectId::DEPTH_LEVELS>(scale);
  result.insert(result.end(), intervals.begin(), intervals.end());
}

MwmContext::MwmContext(MwmSet::MwmHandle handle) : MwmContext(std::move(handle), {} /* type */) {}

MwmContext::MwmContext(MwmSet::MwmHandle handle, MwmType type)
  : m_handle(std::move(handle))
  , m_value(*m_handle.GetValue())
  , m_vector(m_value.m_cont, m_value.GetHeader(), FeaturesTag::Common,
             m_value.GetTable(FeaturesTag::Common))
  , m_index(m_value.m_cont.GetReader(GetIndexTag(FeaturesTag::Common)), m_value.m_factory)
  , m_centers(m_value)
  , m_editableSource(m_handle)
  , m_type(type)
{
}

std::unique_ptr<FeatureType> MwmContext::GetFeature(uint32_t index) const
{
  std::unique_ptr<FeatureType> ft;
  switch (GetEditedStatus(index))
  {
  case FeatureStatus::Deleted:
  case FeatureStatus::Obsolete:
    return ft;
  case FeatureStatus::Modified:
  case FeatureStatus::Created:
    ft = m_editableSource.GetModifiedFeature(index);
    CHECK(ft, ());
    return ft;
  case FeatureStatus::Untouched:
    auto ft = m_vector.GetByIndex(index);
    CHECK(ft, ());
    ft->SetID(FeatureID(GetId(), index));
    return ft;
  }
  UNREACHABLE();
}
}  // namespace search
