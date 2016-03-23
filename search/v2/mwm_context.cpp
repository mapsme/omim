#include "search/v2/mwm_context.hpp"


namespace search
{
namespace v2
{

void CoverRect(m2::RectD const & rect, int scale, covering::IntervalsT & result)
{
  covering::CoveringGetter covering(rect, covering::ViewportWithLowLevels);
  auto const & intervals = covering.Get(scale);
  result.insert(result.end(), intervals.begin(), intervals.end());
}

MwmContext::MwmContext(MwmSet::MwmHandle handle)
  : m_handle(move(handle))
  , m_value(*m_handle.GetValue<MwmValue>())
  , m_vector(m_value.m_cont, m_value.GetHeader(), m_value.m_table)
  , m_index(m_value.m_cont.GetReader(INDEX_FILE_TAG), m_value.m_factory)
{
}

bool MwmContext::GetFeature(uint32_t index, FeatureType & ft) const
{
  switch (GetEditedStatus(index))
  {
  case osm::Editor::FeatureStatus::Deleted:
    return false;
  case osm::Editor::FeatureStatus::Modified:
  case osm::Editor::FeatureStatus::Created:
    VERIFY(osm::Editor::Instance().GetEditedFeature(GetId(), index, ft), ());
    return true;
  case osm::Editor::FeatureStatus::Untouched:
    m_vector.GetByIndex(index, ft);
    ft.SetID(FeatureID(GetId(), index));
    return true;
  }
}

bool MwmContext::GetStreetIndex(uint32_t houseId, vector<ReverseGeocoder::Street> const & streets, uint32_t & streetId)
{
  if (!m_houseToStreetTable)
  {
    m_houseToStreetTable = HouseToStreetTable::Load(m_value);
    ASSERT(m_houseToStreetTable, ());
  }
  uint32_t index;
  m_houseToStreetTable->Get(houseId, index);
  
  if (index < streets.size())
  {
    streetId = streets[index].m_id.m_index;
    LOG(LINFO, ("index in bounds"));
    return true;
  }
  else
  {
    uint32_t const bit = static_cast<uint32_t>(1) << 31;
    if ((index & bit) > 0)
    {
      streetId = index ^ bit;
      LOG(LINFO, ("hacky part!"));

      FeatureType h;
      m_vector.GetByIndex(streetId, h);
      LOG(LINFO, ("idx =", streetId, "ft =", h));
      return true;
    }
  }
  return false;
}

}  // namespace v2
}  // namespace search
