#pragma once

#include "generator/feature_builder.hpp"
#include "generator/gen_mwm_info.hpp"
#include "generator/platform_helpers.hpp"
#include "generator/utils.hpp"

#include "indexer/classificator.hpp"
#include "indexer/complex/hierarchy_entry.hpp"
#include "indexer/complex/tree_node.hpp"
#include "indexer/composite_id.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/tree_node.hpp"

#include "coding/csv_reader.hpp"

#include "geometry/point2d.hpp"
#include "geometry/tree4d.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"
#include "base/geo_object_id.hpp"
#include "base/thread_pool_computational.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/geometry.hpp>
#include <boost/optional.hpp>

namespace generator
{
namespace hierarchy
{
using GetMainType = std::function<uint32_t(FeatureParams::Types const &)>;
using GetName = std::function<std::string(StringUtf8Multilang const &)>;
using PrintFunction = std::function<std::string(indexer::HierarchyEntry const &)>;

// These are dummy functions.
uint32_t GetTypeDefault(FeatureParams::Types const &);
std::string GetNameDefault(StringUtf8Multilang const &);
std::string PrintDefault(indexer::HierarchyEntry const &);

// The HierarchyPlace class is an abstraction of FeatureBuilder to build a hierarchy of objects.
// It allows you to work with the geometry of points and areas.
class HierarchyPlace
{
public:
  explicit HierarchyPlace(feature::FeatureBuilder const & fb);

  double GetArea() const { return m_area; }
  indexer::CompositeId const & GetCompositeId() const { return m_id; }
  StringUtf8Multilang const & GetName() const { return m_name; }
  FeatureParams::Types const & GetTypes() const { return m_types; }
  m2::RectD const & GetLimitRect() const { return m_rect; }
  m2::PointD const & GetCenter() const { return m_center; }
  bool IsPoint() const { return m_isPoint; }

  bool Contains(HierarchyPlace const & smaller) const;
  bool IsEqualGeometry(HierarchyPlace const & other) const;

private:
  bool Contains(m2::PointD const & point) const;

  indexer::CompositeId m_id;
  StringUtf8Multilang m_name;
  feature::FeatureBuilder::PointSeq m_polygon;
  FeatureParams::Types m_types;
  m2::RectD m_rect;
  double m_area = 0.0;
  bool m_isPoint = false;
  m2::PointD m_center;
};

// The HierarchyLinker class allows you to link nodes into a tree according to the hierarchy.
// The knowledge of geometry only is used here.
class HierarchyLinker
{
public:
  using Node = tree_node::TreeNode<HierarchyPlace>;
  using Tree4d = m4::Tree<Node::Ptr>;

  explicit HierarchyLinker(Node::PtrList && nodes);

  Node::PtrList Link();

private:
  static Tree4d MakeTree4d(Node::PtrList const & nodes);

  Node::Ptr FindPlaceParent(HierarchyPlace const & place);

  Node::PtrList m_nodes;
  Tree4d m_tree;
};

// HierarchyBuilder class filters input elements and builds hierarchy from file *.mwm.tmp.
class HierarchyBuilder
{
public:
  using Node = HierarchyLinker::Node;

  explicit HierarchyBuilder(std::string const & dataFilename);

  void SetGetMainTypeFunction(GetMainType const & getMainType);
  void SetGetNameFunction(GetName const & getName);

  Node::PtrList Build();

protected:
  std::vector<feature::FeatureBuilder> ReadFeatures(std::string const & dataFilename);

  std::string m_dataFullFilename;
  GetMainType m_getMainType = GetTypeDefault;
  GetName m_getName = GetNameDefault;
};

class HierarchyLineEnricher
{
public:
  HierarchyLineEnricher(std::string const & osm2FtIdsPath, std::string const & countryFullPath);

  boost::optional<m2::PointD> GetFeatureCenter(indexer::CompositeId const & id) const;

private:
  OsmID2FeatureID m_osm2FtIds;
  FeatureGetter m_featureGetter;
};

class HierarchyLinesBuilder
{
public:
  HierarchyLinesBuilder(HierarchyBuilder::Node::PtrList && nodes);

  void SetGetMainTypeFunction(GetMainType const & getMainType);
  void SetGetNameFunction(GetName const & getName);
  void SetCountryName(std::string const & name);
  void SetHierarchyLineEnricher(std::shared_ptr<HierarchyLineEnricher> const & enricher);

  std::vector<indexer::HierarchyEntry> GetHierarchyLines();

private:
  m2::PointD GetCenter(HierarchyBuilder::Node::Ptr const & node);
  indexer::HierarchyEntry Transform(HierarchyBuilder::Node::Ptr const & node);

  HierarchyBuilder::Node::PtrList m_nodes;
  GetMainType m_getMainType = GetTypeDefault;
  GetName m_getName = GetNameDefault;
  std::string m_countryName;
  std::shared_ptr<HierarchyLineEnricher> m_enricher;
};

uint32_t GetIdWitBestGeom(std::vector<uint32_t> const & ids, FeatureGetter const & ftGetter);

void OrderIds(std::vector<uint32_t> & ids, FeatureGetter const & ftGetter);
}  // namespace hierarchy
}  // namespace generator
