#include "generator/translator_planet.hpp"

#include "generator/emitter_interface.hpp"
#include "generator/feature_builder.hpp"
#include "generator/generate_info.hpp"
#include "generator/holes.hpp"
#include "generator/intermediate_data.hpp"
#include "generator/osm2type.hpp"
#include "generator/osm_element.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature_visibility.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "geometry/point2d.hpp"

#include "base/assert.hpp"

#include <string>
#include <vector>

namespace generator
{
TranslatorPlanet::TranslatorPlanet(std::shared_ptr<EmitterInterface> emitter,
                                   cache::IntermediateDataReader & holder,
                                   feature::GenerateInfo const & info) :
  m_emitter(emitter),
  m_cache(holder),
  m_coastType(info.m_makeCoasts ? classif().GetCoastType() : 0),
  m_nodeRelations(m_routingTagsProcessor),
  m_wayRelations(m_routingTagsProcessor),
  m_metalinesBuilder(info.GetIntermediateFileName(METALINES_FILENAME)),
  m_cameraNodeProcessor(info.GetIntermediateFileName(CAMERAS_TO_WAYS_FILENAME))
{
  auto const addrFilePath = info.GetAddressesFileName();
  if (!addrFilePath.empty())
    m_addrWriter.reset(new FileWriter(addrFilePath));

  auto const restrictionsFilePath = info.GetIntermediateFileName(RESTRICTIONS_FILENAME);
  if (!restrictionsFilePath.empty())
    m_routingTagsProcessor.m_restrictionWriter.Open(restrictionsFilePath);

  auto const roadAccessFilePath = info.GetIntermediateFileName(ROAD_ACCESS_FILENAME);
  if (!roadAccessFilePath.empty())
    m_routingTagsProcessor.m_roadAccessWriter.Open(roadAccessFilePath);
}

void TranslatorPlanet::EmitElement(OsmElement * p)
{
  CHECK(p, ("Tried to emit a null OsmElement"));

  FeatureParams params;
  switch (p->type)
  {
  case OsmElement::EntityType::Node:
  {
    if (p->m_tags.empty())
      break;

    if (!ParseType(p, params))
      break;

    m2::PointD const pt = MercatorBounds::FromLatLon(p->lat, p->lon);
    EmitPoint(pt, params, osm::Id::Node(p->id));
    break;
  }
  case OsmElement::EntityType::Way:
  {
    FeatureBuilder1 ft;
    m2::PointD pt;
    // Parse geometry.
    for (uint64_t ref : p->Nodes())
    {
      if (!m_cache.GetNode(ref, pt.y, pt.x))
        break;
      ft.AddPoint(pt);
    }

    if (p->Nodes().size() != ft.GetPointsCount())
      break;

    if (ft.GetPointsCount() < 2)
      break;

    if (!ParseType(p, params))
      break;

    ft.SetOsmId(osm::Id::Way(p->id));
    bool isCoastline = (m_coastType != 0 && params.IsTypeExist(m_coastType));

    EmitArea(ft, params, [&] (FeatureBuilder1 & ft)
    {
      // Emit coastline feature only once.
      isCoastline = false;
      HolesProcessor processor(p->id, m_cache);
      m_cache.ForEachRelationByWay(p->id, processor);
      ft.SetAreaAddHoles(processor.GetHoles());
    });

    m_metalinesBuilder(*p, params);
    EmitLine(ft, params, isCoastline);
    break;
  }
  case OsmElement::EntityType::Relation:
  {
    // Check if this is our processable relation. Here we process only polygon relations.
    if (!p->HasTagValue("type", "multipolygon"))
      break;

    if (!ParseType(p, params))
      break;

    HolesRelation helper(m_cache);
    helper.Build(p);

    auto const & holesGeometry = helper.GetHoles();
    auto & outer = helper.GetOuter();

    outer.ForEachArea(true /* collectID */, [&] (FeatureBuilder1::PointSeq const & pts,
                      std::vector<uint64_t> const & ids)
    {
      FeatureBuilder1 ft;

      for (uint64_t id : ids)
        ft.AddOsmId(osm::Id::Way(id));

      for (auto const & pt : pts)
        ft.AddPoint(pt);

      ft.AddOsmId(osm::Id::Relation(p->id));
      EmitArea(ft, params, [&holesGeometry] (FeatureBuilder1 & ft) {
        ft.SetAreaAddHoles(holesGeometry);
      });
    });
    break;
  }
  default:
    break;
  }
}

bool TranslatorPlanet::ParseType(OsmElement * p, FeatureParams & params)
{
  // Get tags from parent relations.
  if (p->IsNode())
  {
    m_nodeRelations.Reset(p->id, p);
    m_cache.ForEachRelationByNodeCached(p->id, m_nodeRelations);
  }
  else if (p->IsWay())
  {
    m_wayRelations.Reset(p->id, p);
    m_cache.ForEachRelationByWayCached(p->id, m_wayRelations);
  }

  // Get params from element tags.
  ftype::GetNameAndType(p, params);
  if (!params.IsValid())
    return false;

  if (p->type == OsmElement::EntityType::Node &&
      ftypes::IsSpeedCamChecker::Instance()(params.m_types))
  {
    m_cameraNodeProcessor.ProcessAndWrite(*p, params, m_cache);
  }

  m_routingTagsProcessor.m_roadAccessWriter.Process(*p);
  return true;
}

void TranslatorPlanet::EmitPoint(m2::PointD const & pt,
                                 FeatureParams params, osm::Id id) const
{
  if (!feature::RemoveNoDrawableTypes(params.m_types, feature::GEOM_POINT))
    return;

  FeatureBuilder1 ft;
  ft.SetCenter(pt);
  ft.SetOsmId(id);
  EmitFeatureBase(ft, params);
}

void TranslatorPlanet::EmitLine(FeatureBuilder1 & ft, FeatureParams params, bool isCoastLine) const
{
  if (!isCoastLine && !feature::RemoveNoDrawableTypes(params.m_types, feature::GEOM_LINE))
    return;

  ft.SetLinear(params.m_reverseGeometry);
  EmitFeatureBase(ft, params);
}

void TranslatorPlanet::EmitArea(FeatureBuilder1 & ft, FeatureParams params,
                                std::function<void(FeatureBuilder1 &)> fn)
{
  using namespace feature;

  // Ensure that we have closed area geometry.
  if (!ft.IsGeometryClosed())
    return;

  if (ftypes::IsTownOrCity(params.m_types))
  {
    auto fb = ft;
    fn(fb);
    m_emitter->EmitCityBoundary(fb, params);
  }

  // Key point here is that IsDrawableLike and RemoveNoDrawableTypes
  // work a bit different for GEOM_AREA.
  if (IsDrawableLike(params.m_types, GEOM_AREA))
  {
    // Make the area feature if it has unique area styles.
    VERIFY(RemoveNoDrawableTypes(params.m_types, GEOM_AREA), (params));
    fn(ft);
    EmitFeatureBase(ft, params);
  }
  else
  {
    // Try to make the point feature if it has point styles.
    EmitPoint(ft.GetGeometryCenter(), params, ft.GetLastOsmId());
  }
}

void TranslatorPlanet::EmitFeatureBase(FeatureBuilder1 & ft,
                                       FeatureParams const & params) const
{
  ft.SetParams(params);
  if (!ft.PreSerialize())
    return;

  std::string addr;
  if (m_addrWriter &&
      ftypes::IsBuildingChecker::Instance()(params.m_types) &&
      ft.FormatFullAddress(addr))
  {
    m_addrWriter->Write(addr.c_str(), addr.size());
  }

  (*m_emitter)(ft);
}
}  // namespace generator
