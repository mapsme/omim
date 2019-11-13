#include "generator/feature_maker.hpp"

#include "generator/holes.hpp"
#include "generator/osm2type.hpp"
#include "generator/osm_element.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature_visibility.hpp"

#include <utility>

using namespace feature;

namespace generator
{
std::shared_ptr<FeatureMakerBase> FeatureMakerSimple::Clone() const
{
  return std::make_shared<FeatureMakerSimple>();
}

void FeatureMakerSimple::ParseNameAndTypes(FeatureBuilder & fb, OsmElement & p) const
{
  ftype::GetNameAndType(&p, fb, [](uint32_t type) { return classif().IsTypeValid(type); });
}

bool FeatureMakerSimple::BuildFromNode(OsmElement & p, feature::FeatureBuilder && fb)
{
  fb.SetCenter(mercator::FromLatLon(p.m_lat, p.m_lon));
  fb.SetOsmId(base::MakeOsmNode(p.m_id));
  m_queue.push(std::move(fb));
  return true;
}

bool FeatureMakerSimple::BuildFromWay(OsmElement & p, feature::FeatureBuilder && fb)
{
  auto const & nodes = p.Nodes();
  if (nodes.size() < 2)
    return false;

  m2::PointD pt;
  for (uint64_t ref : nodes)
  {
    if (!m_cache->GetNode(ref, pt.y, pt.x))
      return false;

    fb.AddPoint(pt);
  }

  fb.SetOsmId(base::MakeOsmWay(p.m_id));
  if (fb.IsGeometryClosed())
    fb.SetArea();
  else
    fb.SetLinear(fb.GetParams().m_reverseGeometry);

  m_queue.push(std::move(fb));
  return true;
}

bool FeatureMakerSimple::BuildFromRelation(OsmElement & p, feature::FeatureBuilder && fb)
{
  HolesRelation helper(m_cache);
  helper.Build(&p);
  auto const & holesGeometry = helper.GetHoles();
  auto & outer = helper.GetOuter();
  auto const size = m_queue.size();
  auto func = [&](FeatureBuilder::PointSeq const & pts, std::vector<uint64_t> const & ids) {
    FeatureBuilder fbTemp = fb;
    for (uint64_t id : ids)
      fbTemp.AddOsmId(base::MakeOsmWay(id));

    for (auto const & pt : pts)
      fbTemp.AddPoint(pt);

    fbTemp.AddOsmId(base::MakeOsmRelation(p.m_id));
    if (!fbTemp.IsGeometryClosed())
      return;

    fbTemp.SetHoles(holesGeometry);
    fbTemp.SetArea();
    m_queue.push(std::move(fbTemp));
  };

  outer.ForEachArea(true /* collectID */, std::move(func));
  return size != m_queue.size();
}

std::shared_ptr<FeatureMakerBase> FeatureMaker::Clone() const
{
  return std::make_shared<FeatureMaker>();
}

void FeatureMaker::ParseNameAndTypes(FeatureBuilder & fb, OsmElement & p) const
{
  ftype::GetNameAndType(&p, fb);
}
}  // namespace generator
