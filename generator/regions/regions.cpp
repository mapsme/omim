#include "generator/regions/regions.hpp"

#include "generator/feature_builder.hpp"
#include "generator/feature_generator.hpp"
#include "generator/generate_info.hpp"
#include "generator/regions/place_point.hpp"
#include "generator/regions/regions.hpp"
#include "generator/regions/node.hpp"
#include "generator/regions/regions_builder.hpp"
#include "generator/regions/regions_fixer.hpp"
#include "generator/regions/to_string_policy.hpp"

#include "platform/platform.hpp"

#include "coding/transliteration.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"
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
namespace
{
class RegionsGenerator
{
public:
  RegionsGenerator(std::string const & pathInRegionsTmpMwm, std::string const & pathInRegionsCollector,
                   std::string const & pathOutRegionsKv, std::string const & pathOutRepackedRegionsTmpMwm,
                   bool verbose, size_t threadsCount)
    : m_pathInRegionsTmpMwm{pathInRegionsTmpMwm}
    , m_pathOutRegionsKv{pathOutRegionsKv}
    , m_pathOutRepackedRegionsTmpMwm{pathOutRepackedRegionsTmpMwm}
    , m_verbose{verbose}
    , m_regionsInfoCollector{pathInRegionsCollector}
  {
    LOG(LINFO, ("Start generating regions from", m_pathInRegionsTmpMwm));
    auto timer = base::Timer{};
    Transliteration::Instance().Init(GetPlatform().ResourcesDir());

    RegionsBuilder::Regions regions = ReadAndFixData();
    RegionsBuilder builder{std::move(regions), threadsCount};
    GenerateRegions(builder);

    LOG(LINFO, ("Finish generating regions.", timer.ElapsedSeconds(), "seconds."));
  }

private:
  void GenerateRegions(RegionsBuilder & builder)
  {
    std::ofstream regionsKv{m_pathOutRegionsKv, std::ofstream::out};
    std::set<base::GeoObjectId> setIds;
    size_t countIds = 0;
    builder.ForEachNormalizedCountry([&](std::string const & name, Node::Ptr const & tree) {
      if (!tree)
        return;

      if (m_verbose)
        DebugPrintTree(tree);

      LOG(LINFO, ("Processing country", name));

      auto jsonPolicy = JsonPolicy{m_verbose};
      ForEachLevelPath(tree, [&](auto && path) {
        auto const & node = path.back();
        auto const id = node->GetData().GetId();
        regionsKv << static_cast<int64_t>(id.GetEncodedId()) << " " << jsonPolicy.ToString(path) << "\n";
        ++countIds;
        if (!setIds.insert(id).second)
          LOG(LWARNING, ("Id alredy exists:", id));
      });
    });

    LOG(LINFO, ("Regions objects key-value for", builder.GetCountryNames().size(),
                "countries storage saved to", m_pathOutRegionsKv));
    LOG(LINFO, (countIds, "total ids.", setIds.size(), "unique ids."));

    // todo(maksimandrianov1): Perhaps this is not the best solution. This is a hot fix. Perhaps it
    // is better to transfer this to index generation(function GenerateRegionsData),
    // or to combine index generation and key-value storage generation in
    // generator_tool(generator_tool.cpp).
    RepackTmpMwm(setIds);
  }

  std::tuple<RegionsBuilder::Regions, PlacePointsMap>
  ReadDatasetFromTmpMwm(std::string const & tmpMwmFilename, RegionInfo & collector)
  {
    RegionsBuilder::Regions regions;
    PlacePointsMap placePointsMap;
    auto const toDo = [&](FeatureBuilder1 const & fb, uint64_t /* currPos */) {
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

  RegionsBuilder::Regions ReadAndFixData()
  {
    RegionsBuilder::Regions regions;
    PlacePointsMap placePointsMap;
    std::tie(regions, placePointsMap) = ReadDatasetFromTmpMwm(m_pathInRegionsTmpMwm, m_regionsInfoCollector);
    FixRegionsWithPlacePointApproximation(placePointsMap, regions);
    FilterRegions(regions);
    return regions;
  }

  void FilterRegions(RegionsBuilder::Regions & regions)
  {
    auto const pred = [](Region const & region) {
      auto const & name = region.GetName();
      if (name.empty())
        return false;

      auto const level = RegionsBuilder::GetLevel(region);
      return level != PlaceLevel::Unknown;
    };

    base::EraseIf(regions, pred);
  }

  void RepackTmpMwm(std::set<base::GeoObjectId> const & ids)
  {
    feature::FeaturesCollector collector(m_pathOutRepackedRegionsTmpMwm);
    auto const toDo = [this, &collector, &ids](FeatureBuilder1 & fb, uint64_t /* currPos */) {
      if (ids.count(fb.GetMostGenericOsmId()) == 0 ||
          (fb.IsPoint() && !FeaturePlacePointToRegion(m_regionsInfoCollector, fb)))
      {
        return;
      }

      CHECK(fb.IsArea(), ());
      collector(fb);
    };

    feature::ForEachFromDatRawFormat(m_pathInRegionsTmpMwm, toDo);

    LOG(LINFO, ("Repacked regions temporary mwm saved to", m_pathOutRepackedRegionsTmpMwm));
  }

  std::string m_pathInRegionsTmpMwm;
  std::string m_pathOutRegionsKv;
  std::string m_pathOutRepackedRegionsTmpMwm;

  bool m_verbose{false};

  RegionInfo m_regionsInfoCollector;
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
