#include "routing/city_boundaries_checker.hpp"

#include "indexer/cities_boundaries_serdes.hpp"
#include "indexer/mwm_set.hpp"
#include "indexer/utils.hpp"

#include "geometry/rect2d.hpp"

#include "coding/file_container.hpp"
#include "coding/reader.hpp"

#include "base/assert.hpp"

#include "defines.hpp"

namespace routing
{
using namespace std;

CityBoundariesChecker::CityBoundariesChecker(DataSource const & dataSource)
{
  CityBoundaries cityBoundaries;
  if (!Load(dataSource, cityBoundaries))
    return;

  if (!cityBoundaries.empty())
    FillTree(cityBoundaries);
}

bool CityBoundariesChecker::InCity(m2::PointD const & point) const
{
  if (m_tree.IsEmpty())
    return false;

  bool result = false;
  m_tree.ForEachInRect(m2::RectD(point, point), [&](indexer::CityBoundary const & cityBoundary) {
    if (result)
      return;

    result = cityBoundary.HasPoint(point);
  });

  return result;
}

bool CityBoundariesChecker::Load(DataSource const & dataSource, CityBoundaries & cityBoundaries)
{
  auto handle = indexer::FindWorld(dataSource);
  if (!handle.IsAlive())
  {
    LOG(LWARNING, ("Can't find World map file."));
    return false;
  }

  auto const & mwmValue = *handle.GetValue<MwmValue>();

  if (!mwmValue.m_cont.IsExist(CITIES_BOUNDARIES_FILE_TAG))
    return false; // World.mwm before cities_boundaries section.

  try
  {
    auto reader = mwmValue.m_cont.GetReader(CITIES_BOUNDARIES_FILE_TAG);
    CHECK(reader.GetPtr() != nullptr, ());
    ReaderSource<ReaderPtr<ModelReader>> source(reader);
    double precision;
    indexer::CitiesBoundariesSerDes::Deserialize(source, cityBoundaries, precision);
    return true;
  }
  catch (Reader::Exception const & e)
  {
    LOG(LERROR, ("Can't read cities boundaries table from the world map:", e.Msg()));
    return false;
  }
}

void CityBoundariesChecker::FillTree(CityBoundaries const & cityBoundaries)
{
  for (auto const & cbv : cityBoundaries)
  {
    for (auto const & cb : cbv)
      m_tree.Add(cb,  m2::RectD(cb.m_bbox.Min(), cb.m_bbox.Max()));
  }
}
}  // namespace routing
