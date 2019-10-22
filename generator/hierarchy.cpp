#include "generator/hierarchy.hpp"

#include "indexer/feature_algo.hpp"

#include "geometry/mercator.hpp"
#include "geometry/rect2d.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iterator>
#include <limits>
#include <numeric>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/register/point.hpp>
#include <boost/geometry/geometries/register/ring.hpp>
#include <boost/geometry/multi/geometries/register/multi_point.hpp>

BOOST_GEOMETRY_REGISTER_POINT_2D(m2::PointD, double, boost::geometry::cs::cartesian, x, y);
BOOST_GEOMETRY_REGISTER_RING(std::vector<m2::PointD>);

using namespace feature;

namespace
{
double CalculateOverlapPercentage(std::vector<m2::PointD> const & lhs,
                                  std::vector<m2::PointD> const & rhs)
{
  if (!boost::geometry::intersects(lhs, rhs))
    return 0.0;

  using BoostPolygon = boost::geometry::model::polygon<m2::PointD>;
  std::vector<BoostPolygon> coll;
  boost::geometry::intersection(lhs, rhs, coll);
  auto const min = std::min(boost::geometry::area(lhs), boost::geometry::area(rhs));
  auto const binOp = [](double x, BoostPolygon const & y) { return x + boost::geometry::area(y); };
  auto const sum = std::accumulate(std::begin(coll), std::end(coll), 0.0, binOp);
  return sum * 100 / min;
}
}  // namespace

namespace generator
{
namespace hierarchy
{
uint32_t GetTypeDefault(FeatureParams::Types const &) { return ftype::GetEmptyValue(); }

std::string GetNameDefault(StringUtf8Multilang const &) { return {}; }

std::string PrintDefault(indexer::HierarchyEntry const &) { return {}; }

HierarchyPlace::HierarchyPlace(FeatureBuilder const & fb)
  : m_id(MakeCompositeId(fb))
  , m_name(fb.GetMultilangName())
  , m_types(fb.GetTypes())
  , m_rect(fb.GetLimitRect())
  , m_center(fb.GetKeyPoint())
{
  if (fb.IsPoint())
  {
    m_isPoint = true;
  }
  else if (fb.IsArea())
  {
    m_polygon = fb.GetOuterGeometry();
    boost::geometry::correct(m_polygon);
    m_area = boost::geometry::area(m_polygon);
  }
}

bool HierarchyPlace::Contains(HierarchyPlace const & smaller) const
{
  if (IsPoint())
    return false;

  if (smaller.IsPoint())
    return Contains(smaller.GetCenter());

  return smaller.GetArea() <= GetArea() &&
      CalculateOverlapPercentage(m_polygon, smaller.m_polygon) > 80.0;
}

bool HierarchyPlace::Contains(m2::PointD const & point) const
{
  return boost::geometry::covered_by(point, m_polygon);
}

bool HierarchyPlace::IsEqualGeometry(HierarchyPlace const & other) const
{
  return IsPoint() ? boost::geometry::equals(m_center, other.m_center)
                   : boost::geometry::equals(m_polygon, other.m_polygon);
}

HierarchyLinker::HierarchyLinker(Node::PtrList && nodes)
  : m_nodes(std::move(nodes)), m_tree(MakeTree4d(m_nodes))
{
}

// static
HierarchyLinker::Tree4d HierarchyLinker::MakeTree4d(Node::PtrList const & nodes)
{
  Tree4d tree;
  for (auto const & n : nodes)
    tree.Add(n, n->GetData().GetLimitRect());
  return tree;
}

HierarchyLinker::Node::Ptr HierarchyLinker::FindPlaceParent(HierarchyPlace const & place)
{
  Node::Ptr parent = nullptr;
  auto minArea = std::numeric_limits<double>::max();
  auto const point = place.GetCenter();
  m_tree.ForEachInRect({point, point}, [&](auto const & candidatePtr) {
    auto const & candidate = candidatePtr->GetData();
    if (place.GetCompositeId() == candidate.GetCompositeId())
      return;
    // Sometimes there can be two places with the same geometry. We must compare their ids
    // to avoid cyclic connections.
    if (place.IsEqualGeometry(candidate))
    {
      if (place.GetCompositeId() < candidate.GetCompositeId())
        parent = candidatePtr;
    }
    else if (candidate.GetArea() < minArea && candidate.Contains(place))
    {
      parent = candidatePtr;
      minArea = candidate.GetArea();
    }
  });
  return parent;
}

HierarchyLinker::Node::PtrList HierarchyLinker::Link()
{
  for (auto & node : m_nodes)
  {
    auto const & place = node->GetData();
    auto const parentPlace = FindPlaceParent(place);
    if (!parentPlace)
      continue;

    tree_node::Link(node, parentPlace);
  }
  return m_nodes;
}

HierarchyBuilder::HierarchyBuilder(std::string const & dataFilename)
  : m_dataFullFilename(dataFilename)
{
}

void HierarchyBuilder::SetGetMainTypeFunction(GetMainType const & getMainType)
{
  m_getMainType = getMainType;
}

void HierarchyBuilder::SetGetNameFunction(GetName const & getName) { m_getName = getName; }

std::vector<feature::FeatureBuilder> HierarchyBuilder::ReadFeatures(
    std::string const & dataFilename)
{
  std::vector<feature::FeatureBuilder> fbs;
  ForEachFromDatRawFormat<serialization_policy::MaxAccuracy>(
        dataFilename, [&](FeatureBuilder const & fb, uint64_t /* currPos */) {
    if (m_getMainType(fb.GetTypes()) != ftype::GetEmptyValue() &&
        !fb.GetOsmIds().empty() &&
        (fb.IsPoint() || fb.IsArea()))
    {
      fbs.emplace_back(fb);
    }
  });
  return fbs;
}

HierarchyBuilder::Node::PtrList HierarchyBuilder::Build()
{
  auto const fbs = ReadFeatures(m_dataFullFilename);
  Node::PtrList places;
  places.reserve(fbs.size());
  std::transform(std::cbegin(fbs), std::cend(fbs), std::back_inserter(places),
                 [](auto const & fb) { return std::make_shared<Node>(HierarchyPlace(fb)); });
  return HierarchyLinker(std::move(places)).Link();
}

HierarchyLineEnricher::HierarchyLineEnricher(std::string const & osm2FtIdsPath,
                                             std::string const & countryFullPath)
  : m_featureGetter(countryFullPath)
{
  CHECK(m_osm2FtIds.ReadFromFile(osm2FtIdsPath), (osm2FtIdsPath));
}

boost::optional<m2::PointD> HierarchyLineEnricher::GetFeatureCenter(indexer::CompositeId const & id) const
{
  auto const ids = m_osm2FtIds.GetFeatureIds(id);
  if (ids.empty())
    return {};

  auto const ftId = GetIdWitBestGeom(ids, m_featureGetter);
  auto const ftPtr = m_featureGetter.GetFeatureByIndex(ftId);
  return ftPtr ? feature::GetCenter(*ftPtr) : boost::optional<m2::PointD>();
}

HierarchyLinesBuilder::HierarchyLinesBuilder(HierarchyBuilder::Node::PtrList && nodes)
  : m_nodes(std::move(nodes))
{
}

void HierarchyLinesBuilder::SetGetMainTypeFunction(GetMainType const & getMainType)
{
  m_getMainType = getMainType;
}

void HierarchyLinesBuilder::SetGetNameFunction(GetName const & getName) { m_getName = getName; }

void HierarchyLinesBuilder::SetCountryName(std::string const & name) { m_countryName = name; }

void HierarchyLinesBuilder::SetHierarchyLineEnricher(
    std::shared_ptr<HierarchyLineEnricher> const & enricher)
{
  m_enricher = enricher;
}

std::vector<indexer::HierarchyEntry> HierarchyLinesBuilder::GetHierarchyLines()
{
  std::vector<indexer::HierarchyEntry> lines;
  lines.reserve(m_nodes.size());
  std::transform(std::cbegin(m_nodes), std::cend(m_nodes), std::back_inserter(lines),
                 [&](auto const & n) { return Transform(n); });
  return lines;
}

m2::PointD HierarchyLinesBuilder::GetCenter(HierarchyBuilder::Node::Ptr const & node)
{
  auto const & data = node->GetData();
  if (!m_enricher)
    return data.GetCenter();

  auto const optCenter = m_enricher->GetFeatureCenter(data.GetCompositeId());
  return optCenter ? *optCenter : data.GetCenter();
}

indexer::HierarchyEntry HierarchyLinesBuilder::Transform(HierarchyBuilder::Node::Ptr const & node)
{
  indexer::HierarchyEntry line;
  auto const & data = node->GetData();
  line.m_id = data.GetCompositeId();
  auto const parent = node->GetParent();
  if (parent)
    line.m_parentId = parent->GetData().GetCompositeId();

  line.m_countryName = m_countryName;
  line.m_depth = GetDepth(node);
  line.m_name = m_getName(data.GetName());
  line.m_type = m_getMainType(data.GetTypes());
  line.m_center = GetCenter(node);
  return line;
}

uint32_t GetIdWitBestGeom(std::vector<uint32_t> const & ids, FeatureGetter const & ftGetter)
{
  auto bestGeom = GeomType::Undefined;
  uint32_t bestId = 0;
  for (auto id : ids)
  {
    auto const ftPtr = ftGetter.GetFeatureByIndex(id);
    if (!ftPtr)
      continue;

    auto const geom = ftPtr->GetGeomType();
    switch (geom)
    {
    case GeomType::Point:
      return bestId;
    case GeomType::Line:
    {
      if (bestGeom != GeomType::Point && bestGeom != GeomType::Area)
      {
        bestId = id;
        bestGeom = geom;
      }
    }
      break;
    case GeomType::Area:
    {
      bestId = id;
      bestGeom = geom;
    }
      break;
    default:
      UNREACHABLE();
    }
  }
  return bestId;
}

void OrderIds(std::vector<uint32_t> & ids, FeatureGetter const & ftGetter)
{
  auto idx = std::numeric_limits<size_t>::max();
  for (size_t i = 0; i <  ids.size(); ++i)
  {
    std::unique_ptr<FeatureType> ft = ftGetter.GetFeatureByIndex(ids[i]);
    if (!ft)
      continue;

    if (ft->GetGeomType() == GeomType::Line)
      idx = i;

    bool hasOutlineType = false;
    ft->ForEachType([&](auto t) {
      static auto const outline = classif().GetTypeByPath({"outline"});
      if (t == outline)
        hasOutlineType = true;
    });
    if (hasOutlineType)
    {
      idx = i;
      break;
    }
  }

  if (idx < ids.size())
    std::swap(ids[idx], ids[0]);
}
}  // namespace hierarchy
}  // namespace generator
