#include "generator/popularity.hpp"

#include "generator/boost_helpers.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature_utils.hpp"

#include "geometry/mercator.hpp"

#include "base/stl_helpers.hpp"

#include <algorithm>
#include <functional>
#include <fstream>
#include <limits>
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
  boost_helpers::FillPolygon(*m_polygon, m_feature);
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
  tree.ForEachInRect({point, point}, [&](base::GeoObjectId const & id) {
    if (m.count(id) == 0)
      return;

    auto const & r = m.at(id)->GetData();
    if (r.GetArea() < minArea && r.Contains(point))
    {
      minArea = r.GetArea();
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

    if (r.GetArea() < minArea && r.Contains(place))
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
  std::transform(std::begin(features), std::end(features), std::back_inserter(nodes), [](FeatureBuilder1 const & f) {
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
  auto const it = std::find_if(std::begin(types), std::end(types), std::cref(m_checker));
  return it == std::end(types) ? string() : c.GetReadableObjectName(*it);
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
                               Node::PtrList & geomObjsPtrs, Tree4d & tree, MapIdToNode & mapIdToNode) const
{
  std::vector<FeatureBuilder1> geomObjs;
  feature::ForEachFromDatRawFormat(dataFilename, [&](FeatureBuilder1 const & fb, uint64_t /* currPos */) {
    if (!m_checker(fb.GetTypesHolder()) || GetFeatureName(fb).empty())
      return;

    if (fb.IsPoint())
      pointObjs.push_back(fb);
    else if (fb.IsArea() && fb.IsGeometryClosed())
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

Builder::Builder() : Builder("") {}

std::vector<HierarchyLine> Builder::Build() const
{
  std::vector<FeatureBuilder1> pointObjs;
  Node::PtrList geomObjsPtrs;
  Tree4d tree;
  MapIdToNode mapIdToNode;
  Prepare(m_dataFullFilename, pointObjs, geomObjsPtrs, tree, mapIdToNode);

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
    stream << line.m_id.GetEncodedId() << ";";
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

Builder::Builder() : Builder("") {}

std::vector<HierarchyLine> Builder::Build() const
{
  std::vector<FeatureBuilder1> pointObjs;
  Node::PtrList geomObjsPtrs;
  Tree4d tree;
  MapIdToNode mapIdToNode;
  Prepare(m_dataFilename, pointObjs, geomObjsPtrs, tree, mapIdToNode);

  std::vector<HierarchyLine> result;
  FillLinesFromGeomObjectPtrs(geomObjsPtrs, result);

  std::vector<HierarchyLine> tmp;
  FillLinesFromPointObjects(pointObjs, mapIdToNode, tree, tmp);
  std::copy_if(std::begin(tmp), std::end(tmp), std::back_inserter(result), [] (auto & p) {
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
  stream << "Id;Parent id;Lat;Lon;Main type;Name;MwmName\n";
  for (auto const & line : lines)
  {
    stream << line.m_id.GetEncodedId() << ";";
    if (line.m_parent)
      stream << *line.m_parent;

    auto const center = MercatorBounds::ToLatLon(line.m_center);
    stream << ";" << center.lat << ";" << center.lon << ";"
           << line.m_type << ";" << line.m_name << ";" << line.m_dataFilename << "\n";
  }
}
}  // namespace complex_area
}  // namespace generator
