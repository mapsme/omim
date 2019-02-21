#include "generator/hierarchy.hpp"

#include "generator/boost_helpers.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature_utils.hpp"

#include "geometry/mercator.hpp"

#include "base/stl_helpers.hpp"

#include <algorithm>
#include <functional>
#include <fstream>
#include <limits>
#include <map>
#include <memory>

namespace generator
{
namespace hierarchy
{
HierarchyGeomPlace::HierarchyGeomPlace(FeatureBuilder1 const & feature)
  : m_id(feature.GetMostGenericOsmId())
  , m_feature(feature)
  , m_polygon(std::make_unique<BoostPolygon>())
{
  CHECK(m_polygon, ());
  boost_helpers::FillPolygon(*m_polygon, m_feature, false /* fillHoles */);
  m_area = boost::geometry::area(*m_polygon);
}

bool HierarchyGeomPlace::Contains(HierarchyGeomPlace const & smaller) const
{
  CHECK(m_polygon, ());
  CHECK(smaller.m_polygon, ());

  return GetFeature().GetLimitRect().IsRectInside(smaller.GetFeature().GetLimitRect()) &&
      boost::geometry::covered_by(*smaller.m_polygon, *m_polygon);
}

bool HierarchyGeomPlace::Contains(m2::PointD const & point) const
{
  CHECK(m_polygon, ());

  return GetFeature().GetLimitRect().IsPointInside(point) &&
      boost::geometry::covered_by(BoostPoint(point.x, point.y), *m_polygon);
}

// static
uint8_t HierarchyBuilder::GetLevel(Node::Ptr node)
{
  auto const tmp = node;
  uint8_t level = 0;
  while (node)
  {
    node = node->GetParent();
    ++level;
  }

  return level;
}

// static
std::string HierarchyBuilder::GetFeatureName(FeatureBuilder1 const & feature)
{
  auto const & str = feature.GetParams().name;
  auto const deviceLang = StringUtf8Multilang::GetLangIndex("ru");
  std::string result;
  feature::GetReadableName({}, str, deviceLang, false /* allowTranslit */, result);
  std::replace(std::begin(result), std::end(result), ';', ',');
  std::replace(std::begin(result), std::end(result), '\n', ',');
  return result;
}

// static
boost::optional<base::GeoObjectId>
HierarchyBuilder::FindPointParent(m2::PointD const & point, MapIdToNode const & m, Tree4d const & tree)
{
  boost::optional<base::GeoObjectId> bestId;
  auto minArea = std::numeric_limits<double>::max();
  base::GeoObjectId minId;
  tree.ForEachInRect({point, point}, [&](base::GeoObjectId const & id) {
    if (m.count(id) == 0)
      return;

    auto const & r = m.at(id)->GetData();
    if ((r.GetArea() == minArea ? minId < r.GetId() : r.GetArea() < minArea) && r.Contains(point))
    {
      minArea = r.GetArea();
      minId = r.GetId();
      bestId = id;
    }
  });

  return bestId;
}

// static
boost::optional<HierarchyBuilder::Node::Ptr>
HierarchyBuilder::FindPopularityGeomPlaceParent(HierarchyGeomPlace const & place,
                                                MapIdToNode const & m, Tree4d const & tree)
{
  boost::optional<Node::Ptr> bestPlace;
  auto minArea = std::numeric_limits<double>::max();
  auto const point = place.GetFeature().GetKeyPoint();
  tree.ForEachInRect({point, point}, [&](base::GeoObjectId const & id) {
    if (m.count(id) == 0)
      return;

    auto const & r = m.at(id)->GetData();
    if (r.GetId() == place.GetId() || r.GetArea() < place.GetArea())
      return;

    if ((r.GetArea() == place.GetArea() ? place.GetId() < r.GetId() : r.GetArea() < minArea) &&
        r.Contains(place))
    {
      minArea = r.GetArea();
      bestPlace = m.at(id);
    }
  });

  return bestPlace;
}

// static
HierarchyBuilder::MapIdToNode HierarchyBuilder::GetAreaMap(Node::PtrList const & nodes)
{
  std::unordered_map<base::GeoObjectId, Node::Ptr> result;
  result.reserve(nodes.size());
  for (auto const & n : nodes)
  {
    auto const & d = n->GetData();
    result.emplace(d.GetId(), n);
  }

  return result;
}

// static
HierarchyBuilder::Tree4d HierarchyBuilder::MakeTree4d(Node::PtrList const & nodes)
{
  Tree4d tree;
  for (auto const & n : nodes)
  {
    auto const & data = n->GetData();
    auto const & feature = data.GetFeature();
    tree.Add(data.GetId(), feature.GetLimitRect());
  }

  return tree;
}

// static
void HierarchyBuilder::LinkGeomPlaces(MapIdToNode const & m, Tree4d const & tree, Node::PtrList & nodes)
{
  if (nodes.size() < 2)
    return;

  std::sort(std::begin(nodes), std::end(nodes), [](Node::Ptr const & l, Node::Ptr const & r) {
    return l->GetData().GetArea() < r->GetData().GetArea();
  });

  for (auto & node : nodes)
  {
    auto const & place = node->GetData();
    auto const parentPlace = FindPopularityGeomPlaceParent(place, m, tree);
    if (!parentPlace)
      continue;

    (*parentPlace)->AddChild(node);
    node->SetParent(*parentPlace);
  }
}

// static
HierarchyBuilder::Node::PtrList
HierarchyBuilder::MakeNodes(std::vector<FeatureBuilder1> const & features)
{
  Node::PtrList nodes;
  nodes.reserve(features.size());
  std::transform(std::cbegin(features), std::cend(features), std::back_inserter(nodes), [](FeatureBuilder1 const & f) {
    return std::make_shared<Node>(HierarchyGeomPlace(f));
  });

  return nodes;
}

HierarchyBuilder::HierarchyBuilder(std::string const & dataFilename, ftypes::BaseChecker const & checker)
  : m_dataFullFilename(dataFilename)
  , m_dataFilename(base::GetNameFromFullPathWithoutExt(dataFilename))
  , m_checker(checker) {}

std::string HierarchyBuilder::GetType(FeatureBuilder1 const & feature) const
{
  auto const & c = classif();
  auto const & types = feature.GetTypes();
  auto const it = std::find_if(std::cbegin(types), std::cend(types), std::cref(m_checker));
  return it == std::cend(types) ? string() : c.GetReadableObjectName(*it);
}

void HierarchyBuilder::FillLinesFromPointObjects(std::vector<FeatureBuilder1> const & pointObjs,
                                                 MapIdToNode const & m, Tree4d const & tree,
                                                 std::vector<HierarchyLine> & lines) const
{
  lines.reserve(lines.size() + pointObjs.size());
  for (auto const & p : pointObjs)
  {
    auto const center = p.GetKeyPoint();
    HierarchyLine line;
    line.m_id = p.GetMostGenericOsmId();
    line.m_parent = FindPointParent(center, m, tree);
    line.m_center = center;
    line.m_type = GetType(p);
    line.m_name = GetFeatureName(p);
    line.m_dataFilename = m_dataFilename;
    line.m_level = line.m_parent ? GetLevel(m.at(*line.m_parent)) + 1 : 0;
    lines.push_back(line);
  }
}

void HierarchyBuilder::FillLineFromGeomObjectPtr(HierarchyLine & line, Node::Ptr const & node) const
{
  auto const & data = node->GetData();
  auto const & feature = data.GetFeature();
  line.m_id = data.GetId();
  if (node->HasParent())
    line.m_parent = node->GetParent()->GetData().GetId();

  line.m_center = feature.GetKeyPoint();
  line.m_type = GetType(feature);
  line.m_name = GetFeatureName(feature);
  line.m_dataFilename = m_dataFilename;
  line.m_level = GetLevel(node);
}

void HierarchyBuilder::FillLinesFromGeomObjectPtrs(Node::PtrList const & nodes,
                                                   std::vector<HierarchyLine> & lines) const
{
  lines.reserve(lines.size() + nodes.size());
  for (auto const & n : nodes)
  {
    HierarchyLine line;
    FillLineFromGeomObjectPtr(line, n);
    lines.push_back(line);
  }
}

void HierarchyBuilder::SetDataFilename(std::string const & dataFilename)
{
  m_dataFullFilename = dataFilename;
}

void HierarchyBuilder::Prepare(std::string const & dataFilename, std::vector<FeatureBuilder1> & pointObjs,
                               std::vector<FeatureBuilder1> & geomObjs, Node::PtrList & geomObjsPtrs,
                               Tree4d & tree, MapIdToNode & mapIdToNode) const
{
  feature::ForEachFromDatRawFormat(dataFilename, [&](FeatureBuilder1 const & fb, uint64_t /* currPos */) {
    if (!m_checker(fb.GetTypesHolder()) || GetFeatureName(fb).empty() || fb.GetOsmIds().empty())
      return;

    if (fb.IsPoint())
      pointObjs.push_back(fb);
    else if (fb.IsArea())
      geomObjs.push_back(fb);
  });

  geomObjsPtrs = MakeNodes(geomObjs);
  tree = MakeTree4d(geomObjsPtrs);
  mapIdToNode = GetAreaMap(geomObjsPtrs);
  LinkGeomPlaces(mapIdToNode, tree, geomObjsPtrs);
}
}  // namespace hierarchy

namespace popularity
{
using namespace hierarchy;

Builder::Builder(std::string const & dataFilename) :
  HierarchyBuilder(dataFilename, ftypes::IsPopularityPlaceChecker::Instance()) {}

std::vector<HierarchyLine> Builder::Build() const
{
  std::vector<FeatureBuilder1> pointObjs;
  std::vector<FeatureBuilder1> geomObjs;
  Node::PtrList geomObjsPtrs;
  Tree4d tree;
  MapIdToNode mapIdToNode;
  Prepare(m_dataFullFilename, pointObjs, geomObjs, geomObjsPtrs, tree, mapIdToNode);

  std::vector<HierarchyLine> result;
  FillLinesFromGeomObjectPtrs(geomObjsPtrs, result);
  FillLinesFromPointObjects(pointObjs, mapIdToNode, tree, result);
  return result;
}

// static
void Writer::WriteLines(std::vector<hierarchy::HierarchyLine> const & lines,
                        std::string const & outFilename)
{
  std::ofstream stream;
  stream.exceptions(std::fstream::failbit | std::fstream::badbit);
  stream.open(outFilename);
  stream << std::fixed << std::setprecision(7);
  stream << "Id;Parent id;Lat;Lon;Main type;Name\n";
  for (auto const & line : lines)
  {
    stream << line.m_id << ";";
    if (line.m_parent)
      stream << *line.m_parent;

    auto const center = MercatorBounds::ToLatLon(line.m_center);
    stream << ";" << center.lat << ";" << center.lon << ";"
           << line.m_type << ";" << line.m_name << "\n";
  }
}
}  // namespace popularity

namespace complex_area
{
using namespace hierarchy;

Builder::Builder(std::string const & dataFilename) :
  HierarchyBuilder(dataFilename, ftypes::IsComplexChecker::Instance()) {}

std::string Builder::GetType(FeatureBuilder1 const & feature) const
{
  auto const & c = classif();
  auto const & types = feature.GetTypes();

  auto it = std::find_if(std::cbegin(types), std::cend(types), std::cref(ftypes::IsPoiChecker::Instance()));
  if (it != std::cend(types))
    return c.GetReadableObjectName(*it);

  it = std::find_if(std::cbegin(types), std::cend(types), [&](auto const & t) {
    return m_checker(t) && c.GetReadableObjectName(t) != "building";
  });

  return it == std::cend(types) ? string() : c.GetReadableObjectName(*it);
}

std::vector<HierarchyLine> Builder::Build() const
{
  std::vector<FeatureBuilder1> pointObjs;
  std::vector<FeatureBuilder1> geomObjs;
  Node::PtrList geomObjsPtrs;
  Tree4d tree;
  MapIdToNode mapIdToNode;
  Prepare(m_dataFullFilename, pointObjs, geomObjs, geomObjsPtrs, tree, mapIdToNode);

  std::vector<HierarchyLine> result;
  FillLinesFromGeomObjectPtrs(geomObjsPtrs, result);

  std::vector<HierarchyLine> tmp;
  FillLinesFromPointObjects(pointObjs, mapIdToNode, tree, tmp);
  std::copy_if(std::cbegin(tmp), std::cend(tmp), std::back_inserter(result), [] (auto & p) {
    return static_cast<bool>(p.m_parent);
  });
  return result;
}

// static
void Writer::WriteLines(std::vector<hierarchy::HierarchyLine> const & lines,
                        std::string const & outFilename)
{
  std::ofstream stream;
  stream.exceptions(std::fstream::failbit | std::fstream::badbit);
  stream.open(outFilename);
  stream << std::fixed << std::setprecision(7);
  std::map<uint8_t, size_t> statistic;
  stream << "Id;Parent id;Lat;Lon;Main type;Name;MwmName;Level\n";
  for (auto const & line : lines)
  {
    stream << line.m_id.GetEncodedId() << ";";
    if (line.m_parent)
      stream << *line.m_parent;

    auto const center = MercatorBounds::ToLatLon(line.m_center);
    stream << ";" << center.lat << ";" << center.lon << ";"
           << line.m_type << ";" << line.m_name << ";"
           << line.m_dataFilename << ";" << static_cast<int>(line.m_level) <<"\n";

    statistic[line.m_level] += 1;
  }

  LOG(LINFO, ("Сompleted building file", outFilename));
  for (auto const & item : statistic)
    LOG(LINFO, (item.second, "elements on the level", static_cast<size_t>(item.first)));
}
}  // namespace complex_area
}  // namespace generator
