#pragma once

#include "generator/feature_builder.hpp"
#include "generator/place_node.hpp"
#include "generator/platform_helpers.hpp"

#include "indexer/ftypes_matcher.hpp"

#include "geometry/point2d.hpp"
#include "geometry/tree4d.hpp"

#include "coding/file_name_utils.hpp"

#include "base/assert.hpp"
#include "base/geo_object_id.hpp"
#include "base/thread_pool_computational.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/geometry.hpp>
#include <boost/optional.hpp>

namespace generator_tests
{
class TestPopularityBuilder;
}  // namespace generator_tests

namespace generator
{
namespace hierarchy
{
class HierarchyGeomPlace
{
public:
  explicit HierarchyGeomPlace(FeatureBuilder1 const & feature);

  bool Contains(HierarchyGeomPlace const & smaller) const;
  bool Contains(m2::PointD const & point) const;
  FeatureBuilder1 const & GetFeature() const { return m_feature; }
  void DeletePolygon() { m_polygon = nullptr; }
  double GetArea() const { return m_area; }
  base::GeoObjectId GetId() const { return m_id; }

private:
  using BoostPoint = boost::geometry::model::point<double, 2, boost::geometry::cs::cartesian>;
  using BoostPolygon = boost::geometry::model::polygon<BoostPoint>;

  base::GeoObjectId m_id;
  std::reference_wrapper<FeatureBuilder1 const> m_feature;
  std::unique_ptr<BoostPolygon> m_polygon;
  double m_area;
};

struct HierarchyLine
{
  base::GeoObjectId m_id;
  boost::optional<base::GeoObjectId> m_parent;
  m2::PointD m_center;
  std::string m_type;
  std::string m_name;
  std::string m_dataFilename;
};

class HierarchyBuilder
{
public:
  friend class generator_tests::TestPopularityBuilder;

  explicit HierarchyBuilder(std::string const & dataFilename, ftypes::BaseChecker const & checker);
  virtual ~HierarchyBuilder() = default;

  virtual std::vector<HierarchyLine> Build() const = 0;

protected:
  using Node = PlaceNode<HierarchyGeomPlace>;
  using Tree4d = m4::Tree<base::GeoObjectId>;
  using MapIdToNode = std::unordered_map<base::GeoObjectId, Node::Ptr>;

  static std::string GetFeatureName(FeatureBuilder1 const & feature);
  static boost::optional<base::GeoObjectId>
  FindPointParent(m2::PointD const & point, MapIdToNode const & m, Tree4d const & tree);
  static boost::optional<Node::Ptr>
  FindPopularityGeomPlaceParent(HierarchyGeomPlace const & place, MapIdToNode const & m,
                                Tree4d const & tree);
  static MapIdToNode GetAreaMap(Node::PtrList const & nodes);
  static Tree4d MakeTree4d(Node::PtrList const & nodes);
  static void LinkGeomPlaces(MapIdToNode const & m, Tree4d const & tree, Node::PtrList & nodes);
  static Node::PtrList MakeNodes(std::vector<FeatureBuilder1> const & features);

  std::string GetType(FeatureBuilder1 const & feature) const;
  void FillLinesFromPointObjects(std::vector<FeatureBuilder1> const & pointObjs, MapIdToNode const & m,
                                 Tree4d const & tree, std::vector<HierarchyLine> & lines) const;
  void FillLineFromGeomObjectPtr(HierarchyLine & line, Node::Ptr const & node) const;
  void FillLinesFromGeomObjectPtrs(Node::PtrList const & nodes,
                                   std::vector<HierarchyLine> & lines) const;
  void SetDataFilename(std::string const & dataFilename);
  void Prepare(std::string const & dataFilename, std::vector<FeatureBuilder1> & pointObjs,
               Node::PtrList & geomObjsPtrs, Tree4d & tree, MapIdToNode & mapIdToNode) const;

  std::string m_dataFullFilename;
  std::string m_dataFilename;
  ftypes::BaseChecker const & m_checker;
};
}  // namespace hierarchy

namespace popularity
{
class Builder : public hierarchy::HierarchyBuilder
{
public:
  Builder();
  explicit Builder(std::string const & dataFilename);

  // hierarchy::HierarchyBuilder overrides:
  std::vector<hierarchy::HierarchyLine> Build() const override;
};

class Writer
{
public:
  // Csv format:
  // Id;Parent id;Lat;Lon;Main type;Name
  // 9223372036936022489;;42.996411;41.004747;leisure-park;Сквер им. И. А. Когония
  // 9223372037297546235;;43.325002;40.224941;leisure-park;Приморский парк
  // 9223372036933918763;;43.005177;41.022295;leisure-park;Сухумский ботанический сад
  static void WriteLines(std::vector<hierarchy::HierarchyLine> const & lines,
                         std::string const & outFilename);
};

class Popularity
{
public:
  using Builder = popularity::Builder;
  using Writer = popularity::Writer;
};
}  // namespace popularity

namespace complex_area
{
class Builder : public hierarchy::HierarchyBuilder
{
public:
  Builder();
  explicit Builder(std::string const & dataFilename);

  // hierarchy::HierarchyBuilder overrides:
  std::vector<hierarchy::HierarchyLine> Build() const override;
};

class Writer
{
public:
  // Csv format:
  // Id;Parent id;Lat;Lon;Main type;Name;MwmName
  // 9223372036933918763;;43.005177;41.022295;leisure-park;Сухумский ботанический сад;Abkhazia
  static void WriteLines(std::vector<hierarchy::HierarchyLine> const & lines,
                         std::string const & outFilename);
};

class Complex
{
public:
  using Builder = complex_area::Builder;
  using Writer = complex_area::Writer;
};
}  // namespace complex_area

namespace hierarchy
{
// These are functions for generating a hierarchy csv file.
// dataFilename - A path to data file
// dataDir - A path to the directory where the data files are located
// dataFilenames - Paths to data files
// cpuCount - A number of processes
// outFilename - A path where the csv file will be saved
template <typename T>
std::vector<HierarchyLine> BuildSrcFromData(std::string const & dataFilename)
{
  typename T::Builder builder(dataFilename);
  return builder.Build();
}

template <typename T>
std::vector<HierarchyLine> BuildSrcFromAllData(std::vector<std::string> const & dataFilenames,
                                               size_t cpuCount)
{
  CHECK_GREATER(cpuCount, 0, ());

  base::thread_pool::computational::ThreadPool threadPool(cpuCount);
  std::vector<std::future<std::vector<HierarchyLine>>> futures;
  for (auto const & filename : dataFilenames)
  {
    auto result = threadPool.Submit(
                    static_cast<std::vector<HierarchyLine>(*)(std::string const &)>(BuildSrcFromData<T>),
                    filename);
    futures.emplace_back(std::move(result));
  }

  std::vector<HierarchyLine> result;
  for (auto & f : futures)
  {
    auto lines = f.get();
    std::move(std::begin(lines), std::end(lines), std::back_inserter(result));
  }

  return result;
}

template <typename T>
void BuildSrcFromData(std::string const & dataFilename, std::string const & outFilename)
{
  auto const lines = BuildSrcFromData<T>(dataFilename);
  T::Writer::WriteLines(lines, outFilename);
}

template <typename T>
void BuildSrcFromAllData(std::string const & dataDir, std::string const & outFilename,
                         size_t cpuCount)
{
  auto const filenames = platform_helpers::GetFullDataTmpFilePaths(dataDir);
  auto const lines = BuildSrcFromAllData<T>(filenames, cpuCount);
  T::Writer::WriteLines(lines, outFilename);
}

template <typename T>
void BuildSrcFromAllDataToDir(std::string const & dataDir, std::string const & outDir,
                              size_t cpuCount)
{
  CHECK_GREATER(cpuCount, 0, ());

  base::thread_pool::computational::ThreadPool threadPool(cpuCount);
  auto const filenames = platform_helpers::GetFullDataTmpFilePaths(dataDir);
  for (auto const & filename : filenames)
  {
    auto const outFilename = base::JoinPath(outDir,
                                            base::GetNameFromFullPathWithoutExt(filename) + ".hrc");
    auto result = threadPool.Submit(
                    static_cast<void(*)(std::string const &, std::string const &)>(BuildSrcFromData<T>),
                    filename, outFilename);
  }
}
}  // namespace hierarchy
}  // namespace generator
