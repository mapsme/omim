#include "generator/regions_kv.hpp"

#include "generator/feature_builder.hpp"
#include "generator/generate_info.hpp"
#include "generator/region_info_collector.hpp"
#include "generator/thread_pool.hpp"

#include "geometry/region2d.hpp"

#include "platform/platform.hpp"

#include "coding/file_name_utils.hpp"

#include "base/geo_object_id.hpp"

#include "defines.hpp"

#include "3party/jansson/myjansson.hpp"

#include <fstream>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace
{
using namespace generator;

struct Region
{
  static constexpr short kNoRank = -1;

  using Point = FeatureBuilder1::PointSeq::value_type;

  Region(const FeatureBuilder1 & fb, const generator::RegionData & rd) :
    m_name(fb.GetParams().name),
    m_region(std::move(fb.GetOuterGeometry())),
    m_regionData(rd),
    m_center(fb.GetGeometryCenter())
  {
    auto const rect = m_region.GetRect();
    m_area = (rect.maxX() - rect.minX()) * (rect.maxY() - rect.minY());
  }

  std::string GetName(int8_t lang = StringUtf8Multilang::kDefaultCode) const
  {
    std::string s;
    VERIFY(m_name.GetString(lang, s) != s.empty(), ());
    return s;
  }

  void RemoveRegionGeometry()
  {
    m_region = std::move(m2::Region<Point>());
  }

  bool IsCountry() const
  {
    static int8_t const kAdmLvlCountry = 2;
    return m_regionData.m_adminLevel == kAdmLvlCountry;
  }

  bool IsContain(const Region & smaller) const
  {
    auto const & region = smaller.m_region.Data();
    using Point = Region::Point;
    auto leftRigth = std::minmax_element(std::begin(region), std::end(region),
                                         [](const Point & l, const Point & r) { return l.x < r.x; });
    auto bottomTop = std::minmax_element(std::begin(region), std::end(region),
                                         [](const Point & l, const Point & r) { return l.y < r.y; });

    return m_region.Contains(*leftRigth.first) &&
        m_region.Contains(*leftRigth.second) &&
        m_region.Contains(*bottomTop.first) &&
        m_region.Contains(*bottomTop.second);
  }

  short GetRank() const
  {
    if (m_regionData.m_adminLevel >= 2 && m_regionData.m_adminLevel <= 4)
      return m_regionData.m_adminLevel;

    switch (m_regionData.m_place)
    {
    case PlaceType::City: return 10;
    case PlaceType::Town: return 11;
    case PlaceType::Village: return 13;
    default:
      break;
    }

    if (m_regionData.m_adminLevel >= 5 && m_regionData.m_adminLevel <= 8)
      return m_regionData.m_adminLevel;

    switch (m_regionData.m_place)
    {
    case PlaceType::Suburb: return 14;
    case PlaceType::Neighbourhood: return 15;
    case PlaceType::Hamlet: return 17;
    case PlaceType::Locality: return 19;
    case PlaceType::IsolatedDwelling: return 20;
    default:
      break;
    }

    return kNoRank;
  }

  std::string GetLabel() const
  {
    switch (m_regionData.m_adminLevel)
    {
    case 2: return "country";
    case 4: return "region";
    default:
      break;
    }

    switch (m_regionData.m_place)
    {
    case PlaceType::City:
    case PlaceType::Town:
    case PlaceType::Village:
    case PlaceType::Hamlet:
      return "locality";
    default:
      break;
    }

    switch (m_regionData.m_adminLevel)
    {
    case 6: return "subregion";
    default:
      break;
    }

    switch (m_regionData.m_place)
    {
    case PlaceType::Suburb:
    case PlaceType::Neighbourhood:
      return "suburb";
    case PlaceType::Locality:
    case PlaceType::IsolatedDwelling:
      return "sublocality";
    default:
      break;
    }

    return "";
  }

  StringUtf8Multilang m_name;
  m2::Region<Point> m_region;
  RegionData m_regionData;
  Point m_center;
  double m_area;
};

struct Node;
using NodePtr = std::shared_ptr<Node>;
using NodeWPtr = std::weak_ptr<Node>;
using NodePtrList = std::vector<NodePtr>;

struct Node
{
  Node(const Region & region) : m_region(region) {}

  Region m_region;
  NodePtrList m_children;
  NodeWPtr m_parent;
};

// This function is for debugging only and can be a collection of statistics.
int SizeTree(NodePtr node)
{
  if (node->m_children.empty())
    return 1;

  int i = 0;
  for (auto const & n : node->m_children)
    i += SizeTree(n);

  return i + 1;
}

// This function is for debugging only and can be a collection of statistics.
int MaxDepth(NodePtr node)
{
  if (node->m_children.empty())
    return 1;

  int i = -1;
  for (auto const & n : node->m_children)
    i = std::max(MaxDepth(n), i);

  return i + 1;
}

class JsonPolicy
{
public:
  std::string ToString(NodePtrList const & nodePtrList) const
  {
    if (nodePtrList.empty())
      return "";

    const auto & main = nodePtrList[0];

    auto geometry = my::NewJSONObject();
    ToJSONObject(*geometry, "type", "Point");
    auto coordinates = my::NewJSONArray();
    ToJSONArray(*coordinates, main->m_region.m_center.x);
    ToJSONArray(*coordinates, main->m_region.m_center.y);
    ToJSONObject(*geometry, "coordinates", coordinates);

    auto address = my::NewJSONObject();
    for (auto const & p : nodePtrList)
      ToJSONObject(*address, p->m_region.GetLabel(), p->m_region.GetName());

    auto properties = my::NewJSONObject();
    ToJSONObject(*properties, "name", main->m_region.GetName());
    ToJSONObject(*properties, "rank", main->m_region.GetRank());
    ToJSONObject(*properties, "address", address);

    auto feature = my::NewJSONObject();
    ToJSONObject(*feature, "type", "Feature");
    ToJSONObject(*feature, "geometry", geometry);
    ToJSONObject(*feature, "properties", properties);

    return json_dumps(feature.release(), JSON_COMPACT);
  }
};

template<typename ToStringPolicy>
class RegionKvBuilder : public ToStringPolicy
{
public:
  using Regions = std::vector<Region>;
  using StringList = std::vector<std::string>;
  using CountryTrees = std::multimap<std::string, NodePtr>;

  explicit RegionKvBuilder(Regions && pregions)
  {
    Regions regions(std::move(pregions));
    std::copy_if(std::begin(regions), std::end(regions), std::back_inserter(m_countries),
                 [](Region const & r){ return r.IsCountry(); });
    auto const it = std::remove_if(std::begin(regions), std::end(regions),
                                   [](Region const & r){ return r.IsCountry(); });
    regions.erase(it, std::end(regions));

    MakeCountryTrees(regions);
  }

  const Regions & GetCountries() const { return m_countries; }

  StringList GetCountryNames() const
  {
    StringList result;
    std::unordered_set<std::string> set;
    for (auto const & c : GetCountries())
    {
      auto const name = c.GetName();
      if (set.count(name))
        continue;

      set.insert(name);
      result.emplace_back(std::move(name));
    }

    return result;
  }

  const CountryTrees & GetCountryTrees() const { return m_cuntryTrees; }

  StringList ToStrings(NodePtr tree) const
  {
    StringList result;
    std::queue<NodePtr> queue;
    queue.push(tree);
    while (!queue.empty())
    {
      const auto el = queue.front();
      queue.pop();
      NodePtrList nodes;
      auto current = el;
      while (current)
      {
        nodes.push_back(current);
        current = current->m_parent.lock();
      }

      result.emplace_back(ToStringPolicy::ToString(nodes));
      for (auto const & n : el->m_children)
        queue.push(n);
    }

    return result;
  }

private:
  static NodePtr BuildCountryRegionTree(const Region & country,  const Regions & allRegions)
  {
    Regions regionsInCountry;
    auto const rectCountry = country.m_region.GetRect();
    std::copy_if(std::begin(allRegions), std::end(allRegions), std::back_inserter(regionsInCountry),
                 [&country, &rectCountry] (const Region & r)
    {
      auto const rect = r.m_region.GetRect();
      return rectCountry.IsRectInside(rect) && !r.IsCountry();
    });

    regionsInCountry.emplace_back(country);
    std::sort(std::begin(regionsInCountry), std::end(regionsInCountry),
              [](const Region & r, const Region & l) { return r.m_area > l.m_area; });

    std::vector<NodePtr> nodes;
    nodes.reserve(regionsInCountry.size());
    std::transform(std::begin(regionsInCountry), std::end(regionsInCountry),std::back_inserter(nodes),
                   [](const Region & r) { return std::make_shared<Node>(r); });

    while (nodes.size() > 1)
    {
      auto itFirstNode = std::rbegin(nodes);
      auto & firstRegion = (*itFirstNode)->m_region;
      auto const & rectFirst = firstRegion.m_region.GetRect();
      auto itCurr = itFirstNode + 1;
      for (; itCurr != std::rend(nodes); ++itCurr)
      {
        auto const & currRegion = (*itCurr)->m_region;
        auto const & rectCurr = currRegion.m_region.GetRect();
        if (rectCurr.IsRectInside(rectFirst) && currRegion.IsContain(firstRegion))
        {
          (*itFirstNode)->m_parent = *itCurr;
          (*itCurr)->m_children.push_back(*itFirstNode);
          firstRegion.RemoveRegionGeometry();
          nodes.pop_back();
          break;
        }
      }

      if (itCurr == std::rend(nodes))
        nodes.pop_back();
    }

    return nodes.empty() ? std::shared_ptr<Node>() : nodes[0];
  }

  void MakeCountryTrees(Regions const & regions)
  {
    std::vector<std::future<NodePtr>> results;
    {
      auto const cpuCount = std::thread::hardware_concurrency();
      ASSERT_GREATER(cpuCount, 0, ());
      ThreadPool threadPool(cpuCount);
      for (auto const & country : GetCountries())
      {
        auto f = threadPool.enqueue(&RegionKvBuilder::BuildCountryRegionTree, country, regions);
        results.emplace_back(std::move(f));
      }
    }

    for (auto & r : results)
    {
      auto const tree = r.get();
      m_cuntryTrees.insert({tree->m_region.GetName(), tree});
    }
  }

  CountryTrees m_cuntryTrees;
  Regions m_countries;
};

using RegionKvBuilderJson = RegionKvBuilder<JsonPolicy>;
}  // namespace

namespace generator
{
bool GenerateRegionsKv(const feature::GenerateInfo & genInfo)
{ 
  auto const collectorFileName = genInfo.GetTmpFileName(genInfo.m_fileName,
                                                        RegionInfoCollector::kDefaultExt);
  RegionInfoCollector regionsInfoCollector(collectorFileName);
  RegionKvBuilderJson::Regions regions;

  auto const tmpMwmFileName = genInfo.GetTmpFileName(genInfo.m_fileName);
  auto callback = [&regions, &regionsInfoCollector](const FeatureBuilder1 & fb, uint64_t)
  {
    if (!fb.IsArea() || !fb.IsGeometryClosed())
      return;

    auto const id = fb.GetLastOsmId().GetSerialId();
    auto const region = Region(fb, regionsInfoCollector.Get(id));

    auto const & label = region.GetLabel();
    auto const & name = region.GetName();
    if (label.empty() || name.empty())
      return;

    regions.emplace_back(region);
  };

  LOG(LINFO, ("Processing", tmpMwmFileName));
  feature::ForEachFromDatRawFormat(tmpMwmFileName, callback);

  auto kvBuilder = std::make_unique<RegionKvBuilderJson>(std::move(regions));
  auto const countryTrees = kvBuilder->GetCountryTrees();
  auto const jsonlName = genInfo.GetIntermediateFileName(genInfo.m_fileName, ".jsonl");
  std::ofstream ofs(jsonlName, std::ofstream::out);
  for (auto const & countryName : kvBuilder->GetCountryNames())
  {
    auto keyRange = countryTrees.equal_range(countryName);
    for (auto it = keyRange.first; it != keyRange.second; ++it)
    {
      auto const strings = kvBuilder->ToStrings(it->second);
      for (auto const & s: strings)
      {
        static uint64_t encodedId = 0;
        ofs << base::GeoObjectId(base::GeoObjectId::Type::OsmRelation, ++encodedId).GetEncodedId()
            << " " << s << std::endl;
      }
    }
  }

  return true;
}
}  // namespace generator
