#include "generator/regions_kv.hpp"

#include "generator/feature_builder.hpp"
#include "generator/generate_info.hpp"
#include "generator/region_info_collector.hpp"

#include "platform/platform.hpp"

#include "coding/file_name_utils.hpp"

#include "base/geo_object_id.hpp"

#include "3party/boost/boost/geometry.hpp"
#include "3party/jansson/myjansson.hpp"
#include "3party/thread_pool.hpp"

#include <fstream>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "defines.hpp"

namespace
{
using namespace generator;

namespace bg = boost::geometry;

struct Region
{
  static uint8_t constexpr kNoRank = 0;

  using Point = FeatureBuilder1::PointSeq::value_type;
  using BoostPoint = bg::model::point<double, 2, bg::cs::cartesian>;
  using BoostPolygon = bg::model::polygon<BoostPoint>;

  Region(FeatureBuilder1 const & fb, RegionDataProxy rd) :
    m_name(fb.GetParams().name),
    m_regionData(rd),
    m_rect(fb.GetLimitRect())
  {
    m_area = m_rect.SizeX() * m_rect.SizeY();
    FillPolygon(fb);
  }

  std::string GetName(int8_t lang = StringUtf8Multilang::kDefaultCode) const
  {
    std::string s;
    VERIFY(m_name.GetString(lang, s) != s.empty(), ());
    return s;
  }

  void ClearPolygon()
  {
    m_polygon = {};
  }

  void FillPolygon(FeatureBuilder1 const & fb)
  {
    auto const & fbGeometry = fb.GetGeometry();
    auto it = std::begin(fbGeometry);
    FillBoostGeometry(m_polygon.outer(), *it);
    m_polygon.inners().resize(fbGeometry.size() - 1);
    int i = 0;
    for (it = ++it; it != std::end(fbGeometry); ++it)
      FillBoostGeometry(m_polygon.inners()[i++], *it);
  }

  bool IsCountry() const
  {
    static auto const kAdminLevelCountry = AdminLevel::Two;
    return m_regionData.GetAdminLevel() == kAdminLevelCountry;
  }

  bool Contains(Region const & smaller) const
  {
    return bg::within(smaller.m_polygon, m_polygon);
  }

  // Absolute rank values do not mean anything. But if the rank of the first object is more than the
  // rank of the second object, then the first object is considered more nested.
  uint8_t GetRank() const
  {
    auto const adminLevel = m_regionData.GetAdminLevel();
    auto const placeType = m_regionData.GetPlaceType();

    switch (adminLevel)
    {
    case AdminLevel::Two:
    case AdminLevel::Three:
    case AdminLevel::Four: return static_cast<uint8_t>(adminLevel);
    default: break;
    }

    switch (placeType)
    {
    case PlaceType::City:
    case PlaceType::Town:
    case PlaceType::Village: return static_cast<uint8_t>(placeType);
    default: break;
    }

    switch (adminLevel) {
    case AdminLevel::Five:
    case AdminLevel::Six:
    case AdminLevel::Seven:
    case AdminLevel::Eight: return static_cast<uint8_t>(adminLevel);
    default: break;
    }

    switch (placeType)
    {
    case PlaceType::Suburb:
    case PlaceType::Neighbourhood:
    case PlaceType::Hamlet:
    case PlaceType::Locality:
    case PlaceType::IsolatedDwelling: return static_cast<uint8_t>(placeType);
    default: break;
    }

    return kNoRank;
  }

  std::string GetLabel() const
  {
    auto const adminLevel = m_regionData.GetAdminLevel();
    auto const placeType = m_regionData.GetPlaceType();

    switch (adminLevel)
    {
    case AdminLevel::Two: return "country";
    case AdminLevel::Four: return "region";
    default: break;
    }

    switch (placeType)
    {
    case PlaceType::City:
    case PlaceType::Town:
    case PlaceType::Village:
    case PlaceType::Hamlet: return "locality";
    default: break;
    }

    switch (adminLevel)
    {
    case AdminLevel::Six: return "subregion";
    default: break;
    }

    switch (placeType)
    {
    case PlaceType::Suburb:
    case PlaceType::Neighbourhood: return "suburb";
    case PlaceType::Locality:
    case PlaceType::IsolatedDwelling: return "sublocality";
    default: break;
    }

    return "";
  }

  bool HasIsoCode() const
  {
    return m_regionData.HasIsoCodeAlpha3();
  }

  std::string GetIsoCode() const
  {
    return m_regionData.GetIsoCodeAlpha3();
  }

  Point Center() const
  {
    return m_rect.Center();
  }

  double GetArea() const
  {
    return m_area;
  }

  m2::RectD const & GetRect() const
  {
    return m_rect;
  }

private:
  template <typename BoostGeometry, typename FbGeometry>
  void FillBoostGeometry(BoostGeometry & geometry, FbGeometry const & fbGeometry)
  {
    for (const auto & p : fbGeometry)
      bg::append(geometry, BoostPoint{p.x, p.y});
  }

  StringUtf8Multilang m_name;
  RegionDataProxy m_regionData;
  BoostPolygon m_polygon;
  m2::RectD m_rect;
  double m_area;
};

struct Node
{
  using Ptr = std::shared_ptr<Node>;
  using WeakPtr = std::weak_ptr<Node>;
  using PtrList = std::vector<Ptr>;

  Node(Region const & region) : m_region(region) {}

  void AddChild(Ptr child)
  {
    m_children.push_back(child);
  }

  void SetParent(Ptr parent)
  {
    m_parent = parent;
  }

  Ptr GetParent()
  {
    return m_parent.lock();
  }

  PtrList const & GetChildren()
  {
    return m_children;
  }

  bool HasChildren()
  {
    return m_children.size();
  }

  Region & GetData()
  {
    return m_region;
  }

private:
  Region m_region;
  PtrList m_children;
  WeakPtr m_parent;
};

// This function is for debugging only and can be a collection of statistics.
size_t TreeSize(Node::Ptr node)
{
  if (!node)
    return 0;

  if (!node->HasChildren())
    return 1;

  size_t i = 0;
  for (auto const & n : node->GetChildren())
    i += TreeSize(n);

  return i + 1;
}

// This function is for debugging only and can be a collection of statistics.
size_t MaxDepth(Node::Ptr node)
{
  if (!node)
    return 0;

  if (!node->HasChildren())
    return 1;

  size_t i = 0;
  for (auto const & n : node->GetChildren())
    i = std::max(MaxDepth(n), i);

  return i + 1;
}

class JsonPolicy
{
public:
  std::string ToString(Node::PtrList const & nodePtrList) const
  {
    if (nodePtrList.empty())
      return "";

    const auto & main = nodePtrList.front()->GetData();
    const auto & country = nodePtrList.back()->GetData();

    auto geometry = my::NewJSONObject();
    ToJSONObject(*geometry, "type", "Point");
    auto coordinates = my::NewJSONArray();
    auto const center = main.Center();
    ToJSONArray(*coordinates, center.x);
    ToJSONArray(*coordinates, center.y);
    ToJSONObject(*geometry, "coordinates", coordinates);

    auto address = my::NewJSONObject();
    for (auto const & p : nodePtrList)
      ToJSONObject(*address, p->GetData().GetLabel(), p->GetData().GetName());

    auto properties = my::NewJSONObject();
    ToJSONObject(*properties, "name", main.GetName());
    ToJSONObject(*properties, "rank", main.GetRank());
    ToJSONObject(*properties, "address", address);
    if (country.HasIsoCode())
      ToJSONObject(*properties, "code", country.GetIsoCode());

    auto feature = my::NewJSONObject();
    ToJSONObject(*feature, "type", "Feature");
    ToJSONObject(*feature, "geometry", geometry);
    ToJSONObject(*feature, "properties", properties);

    return json_dumps(feature.release(), JSON_COMPACT);
  }
};

template<typename ToStringPolicy>
class RegionsKvBuilder : public ToStringPolicy
{
public:
  using Regions = std::vector<Region>;
  using StringList = std::vector<std::string>;
  using CountryTrees = std::multimap<std::string, Node::Ptr>;

  explicit RegionsKvBuilder(Regions && pregions)
  {
    Regions regions(std::move(pregions));
    std::copy_if(std::begin(regions), std::end(regions), std::back_inserter(m_countries),
                 [](Region const & r){ return r.IsCountry(); });
    auto const it = std::remove_if(std::begin(regions), std::end(regions),
                                   [](Region const & r){ return r.IsCountry(); });
    regions.erase(it, std::end(regions));

    MakeCountryTrees(regions);
  }

  Regions const & GetCountries() const
  {
    return m_countries;
  }

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

  CountryTrees const & GetCountryTrees() const
  {
    return m_countryTrees;
  }

  StringList ToStrings(Node::Ptr tree) const
  {
    StringList result;
    std::queue<Node::Ptr> queue;
    queue.push(tree);
    while (!queue.empty())
    {
      const auto el = queue.front();
      queue.pop();
      Node::PtrList nodes;
      auto current = el;
      while (current)
      {
        nodes.push_back(current);
        current = current->GetParent();
      }

      result.emplace_back(ToStringPolicy::ToString(nodes));
      for (auto const & n : el->GetChildren())
        queue.push(n);
    }

    return result;
  }

private:
  static Node::Ptr BuildCountryRegionTree(Region const & country, Regions const & allRegions)
  {
    Regions regionsInCountry;
    std::copy_if(std::begin(allRegions), std::end(allRegions), std::back_inserter(regionsInCountry),
                 [&country] (const Region & r)
    {
      auto const & countryRect = country.GetRect();
      return countryRect.IsRectInside(r.GetRect());
    });

    regionsInCountry.emplace_back(country);
    std::sort(std::begin(regionsInCountry), std::end(regionsInCountry),
              [](const Region & r, const Region & l) { return r.GetArea() > l.GetArea(); });

    Node::PtrList nodes;
    nodes.reserve(regionsInCountry.size());
    std::transform(std::begin(regionsInCountry), std::end(regionsInCountry),std::back_inserter(nodes),
                   [](const Region & r) { return std::make_shared<Node>(r); });

    while (nodes.size() > 1)
    {
      auto itFirstNode = std::rbegin(nodes);
      auto & firstRegion = (*itFirstNode)->GetData();
      auto const & rectFirst = firstRegion.GetRect();
      auto itCurr = itFirstNode + 1;
      for (; itCurr != std::rend(nodes); ++itCurr)
      {
        auto const & currRegion = (*itCurr)->GetData();
        auto const & rectCurr = currRegion.GetRect();
        if (rectCurr.IsRectInside(rectFirst) && currRegion.Contains(firstRegion))
        {
          (*itFirstNode)->SetParent(*itCurr);
          (*itCurr)->AddChild(*itFirstNode);
          // We want to free up memory.
          firstRegion.ClearPolygon();
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
    std::vector<std::future<Node::Ptr>> results;
    {
      auto const cpuCount = std::thread::hardware_concurrency();
      ASSERT_GREATER(cpuCount, 0, ());
      ThreadPool threadPool(cpuCount);
      for (auto const & country : GetCountries())
      {
        auto f = threadPool.enqueue(&RegionsKvBuilder::BuildCountryRegionTree, country, regions);
        results.emplace_back(std::move(f));
      }
    }

    for (auto & r : results)
    {
      auto const tree = r.get();
      m_countryTrees.insert({tree->GetData().GetName(), tree});
    }
  }

  CountryTrees m_countryTrees;
  Regions m_countries;
};

using RegionKvBuilderJson = RegionsKvBuilder<JsonPolicy>;
}  // namespace

namespace generator
{
bool GenerateRegions(feature::GenerateInfo const & genInfo)
{ 
  auto const collectorFilename = genInfo.GetTmpFileName(genInfo.m_fileName,
                                                        RegionInfoCollector::kDefaultExt);
  RegionInfoCollector regionsInfoCollector(collectorFilename);
  RegionKvBuilderJson::Regions regions;

  auto const tmpMwmFilename = genInfo.GetTmpFileName(genInfo.m_fileName);
  auto callback = [&regions, &regionsInfoCollector](const FeatureBuilder1 & fb, uint64_t)
  {
    if (!fb.IsArea() || !fb.IsGeometryClosed())
      return;

    auto const id = fb.GetMostGenericOsmId().GetSerialId();
    auto const region = Region(fb, regionsInfoCollector.Get(id));

    auto const & label = region.GetLabel();
    auto const & name = region.GetName();
    if (label.empty() || name.empty())
      return;

    regions.emplace_back(region);
  };

  LOG(LINFO, ("Processing", tmpMwmFilename));
  feature::ForEachFromDatRawFormat(tmpMwmFilename, callback);

  auto kvBuilder = std::make_unique<RegionKvBuilderJson>(std::move(regions));
  auto const countryTrees = kvBuilder->GetCountryTrees();
  auto const jsonlName = genInfo.GetIntermediateFileName(genInfo.m_fileName, ".jsonl");
  std::ofstream ofs(jsonlName, std::ofstream::out);
  for (auto const & countryName : kvBuilder->GetCountryNames())
  {
    auto const keyRange = countryTrees.equal_range(countryName);
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
