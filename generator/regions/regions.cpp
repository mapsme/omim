#include "generator/regions/regions.hpp"

#include "generator/feature_builder.hpp"
#include "generator/feature_generator.hpp"
#include "generator/generate_info.hpp"
#include "generator/regions/node.hpp"
#include "generator/regions/place_point.hpp"
#include "generator/regions/regions.hpp"
#include "generator/regions/regions_builder.hpp"
#include "generator/regions/to_string_policy.hpp"

#include "platform/platform.hpp"

#include "coding/file_name_utils.hpp"
#include "coding/transliteration.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/stl_helpers.hpp"
#include "base/timer.hpp"

#include <algorithm>
#include <fstream>
#include <numeric>
#include <memory>
#include <queue>
#include <set>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "defines.hpp"

namespace generator
{
namespace regions
{
namespace {
class RegionsGenerator
{
public:
  RegionsGenerator(std::string const & pathInRegionsTmpMwm, std::string const & pathInRegionsCollector,
                   std::string const & pathOutRegionsKv, std::string const & pathOutRepackedRegionsTmpMwm,
                   bool verbose, size_t threadsCount)
    : m_pathInRegionsTmpMwm{pathInRegionsTmpMwm}
    , m_regionsKv{pathOutRegionsKv, std::ofstream::out}
    , m_featuresCollector{pathOutRepackedRegionsTmpMwm}
    , m_verbose{verbose}
  {
    LOG(LINFO, ("Start generating regions from", m_pathInRegionsTmpMwm));
    auto timer = base::Timer();
    Transliteration::Instance().Init(GetPlatform().ResourcesDir());

    RegionInfo regionsInfoCollector(pathInRegionsCollector);
    RegionsBuilder::Regions regions;
    PlacePointsMap placePointsMap;
    std::tie(regions, placePointsMap) = ReadDatasetFromTmpMwm(m_pathInRegionsTmpMwm, regionsInfoCollector);
    RegionsBuilder builder(std::move(regions), std::move(placePointsMap), threadsCount);
    GenerateRegions(builder);

    LOG(LINFO, ("Regions objects key-value for", builder.GetCountryNames().size(),
                "countries storage saved to",  pathOutRegionsKv));
    LOG(LINFO, ("Repacked regions temporary mwm saved to", pathOutRepackedRegionsTmpMwm));
    LOG(LINFO, (m_regionsTotalCount, "total ids.", m_countriesRegionIds.size(), "unique ids."));
    LOG(LINFO, ("Finish generating regions.", timer.ElapsedSeconds(), "seconds."));
  }

private:
  void GenerateRegions(RegionsBuilder & builder)
  {
    std::multimap<base::GeoObjectId, Node::Ptr> objectsRegions;

    builder.ForEachCountry([&, this] (
        std::string const &, Node::PtrList const & outers, CountryRegionsBuilderStats const & stats) {
      if (outers.empty())
        return;

      auto const countryPlace = outers.front()->GetData();
      auto const countryName = countryPlace.GetEnglishOrTransliteratedName();

      GenerateKv(countryName, outers, objectsRegions);
      LogStatistics(countryName, stats);
    });

    // todo(maksimandrianov1): Perhaps this is not the best solution. This is a hot fix. Perhaps it
    // is better to transfer this to index generation(function GenerateRegionsData),
    // or to combine index generation and key-value storage generation in
    // generator_tool(generator_tool.cpp).
    RepackTmpMwm(m_pathInRegionsTmpMwm, objectsRegions);
  }

  void GenerateKv(std::string const & countryName, Node::PtrList const & outers,
                  std::multimap<base::GeoObjectId, Node::Ptr> & objectsRegions)
  {
    LOG(LINFO, ("Generate country", countryName));

    auto country = std::make_shared<std::string>(countryName);
    size_t countryRegionsCount = 0;
    size_t countryRegionObjectCount = 0;

    auto jsonPolicy = std::make_unique<JsonPolicy>(m_verbose);
    for (auto const & tree : outers)
    {
      if (m_verbose)
        DebugPrintTree(tree);

      ForEachLevelPath(tree, [&] (NodePath const & path) {
        ++m_regionsTotalCount;
        ++countryRegionsCount;

        auto const & node = path.back();
        auto const & place = node->GetData();
        auto const & placeId = place.GetId();
        auto const & regionEmplace = m_countriesRegionIds.emplace(placeId, country);
        if (!regionEmplace.second)
        {
          if (regionEmplace.first->second != country)  // object may have several regions
          {
            LOG(LWARNING, ("Failed to place", GetLabel(place.GetLevel()), "region", placeId,
                           "(", GetPlaceNotation(place), ")",
                           "into", *country, ": region exist in", *regionEmplace.first->second,
                           "already"));
          }
          return;
        }

        m_regionsKv << static_cast<int64_t>(placeId.GetEncodedId()) << " " << jsonPolicy->ToString(path)
                    << "\n";
        objectsRegions.emplace(placeId, node);
        ++countryRegionObjectCount;
      });
    }

    LOG(LINFO, ("Country regions of", *country, "has built:", countryRegionsCount, "total ids.",
                countryRegionObjectCount, "object ids."));
  }

  std::tuple<RegionsBuilder::Regions, PlacePointsMap>
  ReadDatasetFromTmpMwm(std::string const & tmpMwmFilename, RegionInfo & collector)
  {
    RegionsBuilder::Regions regions;
    PlacePointsMap placePointsMap;
    auto const toDo = [&](FeatureBuilder1 const & fb, uint64_t /* currPos */) {
      if (fb.GetName().empty())
        return;

      if (fb.IsArea() && fb.IsGeometryClosed())
      {
        auto const id = fb.GetMostGenericOsmId();
        auto region = Region(fb, collector.Get(id));
        regions.emplace_back(std::move(region));
      }
      else if (fb.IsPoint())
      {
        auto const id = fb.GetMostGenericOsmId();
        placePointsMap.emplace(id, PlacePoint{fb, collector.Get(id)});
      }
    };

    feature::ForEachFromDatRawFormat(tmpMwmFilename, toDo);
    return std::make_tuple(std::move(regions), std::move(placePointsMap));
  }

  void RepackTmpMwm(std::string const & srcFilename,
                    std::multimap<base::GeoObjectId, Node::Ptr> const & objectsRegions)
  {
    std::set<base::GeoObjectId> processedObjects;
    auto const toDo = [&](FeatureBuilder1 & fb, uint64_t /* currPos */) {
      auto const id = fb.GetMostGenericOsmId();
      auto objectRegions = objectsRegions.equal_range(id);
      if (objectRegions.first == objectRegions.second)
        return;

      if (!processedObjects.insert(id).second)
        return;

      for (auto region = objectRegions.first; region != objectRegions.second; ++region)
      {
        ResetGeometry(fb, region->second->GetData().GetRegion());
        fb.SetOsmId(id);
        fb.SetRank(0);
        m_featuresCollector(fb);
      }
    };

    feature::ForEachFromDatRawFormat(srcFilename, toDo);
  }

  void ResetGeometry(FeatureBuilder1 & fb, Region const & region)
  {
    fb.ResetGeometry();
    fb.SetAreaAddHoles({});
    auto const & polygon =  region.GetPolygon();
    fb.AddPolygon(GetPointSeq(polygon->outer()));
    for (auto & hole : polygon->inners())
      fb.AddPolygon(GetPointSeq(hole));
    CHECK(fb.IsArea(), ());
  }

  template <typename Polygon>
  FeatureBuilder1::PointSeq GetPointSeq(Polygon const & polygon)
  {
    FeatureBuilder1::PointSeq seq;
    std::transform(std::begin(polygon), std::end(polygon), std::back_inserter(seq),
                   [] (BoostPoint const & p) { return m2::PointD(p.get<0>(), p.get<1>()); });
    return seq;
  }

  void LogStatistics(std::string const & countryName, CountryRegionsBuilderStats const & stats)
  {
    LOG(LINFO, ("-----------------------------------------------------------------"));

    for (auto const & item : stats.objectLevelCounts)
      LOG(LINFO, (countryName, ":", GetLabel(item.first), "label", "-", item.second));

    for (auto const & item : stats.placeCounts)
      LOG(LINFO, (countryName, ":", StringifyPlaceType(item.first), "place", "-", item.second));

    for (auto const placeType : GetPlacePointTypes(stats))
      LogPlacePointStatistics(countryName, stats, placeType);

    for (auto const & item : stats.adminLevels)
      LogStatistics(countryName, item.first, item.second);

    LOG(LINFO, ("-----------------------------------------------------------------"));
  }

  void LogPlacePointStatistics(std::string const & countryName, CountryRegionsBuilderStats const & stats,
                               PlaceType placeType)
  {
    auto countIn = [placeType] (std::map<PlaceType, CountryRegionsBuilderStats::Counter> const & pointsStats) {
      auto i = pointsStats.find(placeType);
      return i != end(pointsStats) ? i->second : 0;
    };  
    auto const totalCount = countIn(stats.placePointsCounts);
    auto const unboundedCount = countIn(stats.unboudedPlacePointsCounts);
    auto const boundedCount = totalCount - unboundedCount;
    LOG(LINFO, (countryName, ":", StringifyPlaceType(placeType), "place point" "-",
                boundedCount, "/", totalCount,
                (totalCount ? "(" + std::to_string(100 * boundedCount / totalCount) + "%)" : "")));
  }

  void LogStatistics(std::string const & countryName, AdminLevel adminLevel,
                     CountryRegionsBuilderStats::AdminLevelStats const & adminLevelStats)
  {
    auto const adminLevelLabel = "admin_level=" + std::to_string(static_cast<int>(adminLevel));

    auto placeSummary = GeneratePlaceSummary(adminLevelStats.placeCounts);
    if (!placeSummary.empty())
      placeSummary = "(" + placeSummary + ")";

    LOG(LINFO, (countryName, ":", adminLevelLabel, "-", adminLevelStats.count, placeSummary));
  }

  std::set<PlaceType> GetPlacePointTypes(CountryRegionsBuilderStats const & stats)
  {
    std::set<PlaceType> placePointTypes;

    for (auto const & placeTypeStats : stats.placePointsCounts)
      placePointTypes.insert(placeTypeStats.first);

    for (auto const & placeTypeStats : stats.placePointsCounts)
      placePointTypes.insert(placeTypeStats.first);

    return placePointTypes;
  }

  template <typename Counts>
  std::string GeneratePlaceSummary(Counts const & placeCounts)
  {
    std::stringstream summary;
    for (auto const & item : placeCounts)
      summary << ", " << StringifyPlaceType(item.first) << " - " << item.second;

    auto summaryStr = summary.str();
    if (!summaryStr.empty())
      summaryStr.erase(0, 2);

    return summaryStr;
  }

  std::string m_pathInRegionsTmpMwm;
  std::ofstream m_regionsKv;
  feature::FeaturesCollector m_featuresCollector;

  std::map<base::GeoObjectId, std::shared_ptr<std::string>> m_countriesRegionIds;
  size_t m_regionsTotalCount = 0;

  bool m_verbose{false};
};
}  // namespace

void GenerateRegions(std::string const & pathInRegionsTmpMwm,
                     std::string const & pathInRegionsCollector,
                     std::string const & pathOutRegionsKv,
                     std::string const & pathOutRepackedRegionsTmpMwm,
                     bool verbose,
                     size_t threadsCount)
{
  RegionsGenerator(pathInRegionsTmpMwm, pathInRegionsCollector,
                   pathOutRegionsKv, pathOutRepackedRegionsTmpMwm,
                   verbose, threadsCount);
}
}  // namespace regions
}  // namespace generator
