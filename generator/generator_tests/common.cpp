#include "generator/generator_tests/common.hpp"

#include "generator/borders.hpp"
#include "generator/osm2type.hpp"

#include "indexer/classificator.hpp"

#include "platform/platform.hpp"

#include "base/file_name_utils.hpp"
#include "base/string_utils.hpp"

#include <fstream>

#include "defines.hpp"

namespace generator_tests
{
OsmElement MakeOsmElement(uint64_t id, Tags const & tags, OsmElement::EntityType t)
{
  OsmElement el;
  el.m_id = id;
  el.m_type = t;
  for (auto const & t : tags)
    el.AddTag(t.first, t.second);

  return el;
}

std::string GetFileName(std::string const & filename)
{
  auto & platform = GetPlatform();
  return filename.empty() ? platform.TmpPathForFile() : platform.TmpPathForFile(filename);
}

bool MakeFakeBordersFile(std::string const & intemediatePath, std::string const & filename)
{
  auto const borderPath = base::JoinPath(intemediatePath, BORDERS_DIR);
  auto & platform = GetPlatform();
  auto const code = platform.MkDir(borderPath);
  if (code != Platform::EError::ERR_OK && code != Platform::EError::ERR_FILE_ALREADY_EXISTS)
    return false;

  std::vector<m2::PointD> points = {{-180.0, -90.0}, {180.0, -90.0}, {180.0, 90.0}, {-180.0, 90.0},
                                    {-180.0, -90.0}};
  borders::DumpBorderToPolyFile(borderPath, filename, {m2::RegionD{points}});
  return true;
}

OsmElement MakeOsmElement(OsmElementData const & elementData)
{
  OsmElement el;
  el.m_id = elementData.m_id;
  el.m_type = elementData.m_polygon.size() > 1 ? OsmElement::EntityType::Relation
                                               : OsmElement::EntityType::Node;
  for (auto const & tag : elementData.m_tags)
    el.AddTag(tag.m_key, tag.m_value);
  el.m_members = elementData.m_members;

  return el;
}

feature::FeatureBuilder FeatureBuilderFromOmsElementData(OsmElementData const & elementData)
{
  auto el = MakeOsmElement(elementData);
  feature::FeatureBuilder fb;
  CHECK(elementData.m_polygon.size() == 1 || elementData.m_polygon.size() == 2, ());
  if (elementData.m_polygon.size() == 1)
  {
    fb.SetCenter(elementData.m_polygon[0]);
  }
  else if (elementData.m_polygon.size() == 2)
  {
    auto const & p1 = elementData.m_polygon[0];
    auto const & p2 = elementData.m_polygon[1];
    vector<m2::PointD> poly = {
      {p1.x, p1.y}, {p1.x, p2.y}, {p2.x, p2.y}, {p2.x, p1.y}, {p1.x, p1.y}};
    fb.AddPolygon(poly);
    fb.SetHoles({});
    fb.SetArea();
  }

  auto osmId = el.m_type == OsmElement::EntityType::Relation ? base::MakeOsmRelation(el.m_id)
                                                             : base::MakeOsmNode(el.m_id);
  fb.SetOsmId(osmId);

  ftype::GetNameAndType(&el, fb.GetParams(),
                        [](uint32_t type) { return classif().IsTypeValid(type); });
  return fb;
}

}  // namespace generator_tests
