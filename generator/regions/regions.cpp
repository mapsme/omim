#include "generator/regions/regions.hpp"

#include "generator/feature_builder.hpp"
#include "generator/feature_generator.hpp"
#include "generator/generate_info.hpp"
#include "generator/regions/node.hpp"
#include "generator/regions/place_center.hpp"
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
struct RegionsGenerator
{
  std::string pathInRegionsTmpMwm;
  std::ofstream regionsKv;
  feature::FeaturesCollector featuresCollector;

  std::map<base::GeoObjectId, std::shared_ptr<std::string>> countriesRegionIds;
  size_t regionsTotalCount = 0;

  bool verbose;

  RegionsGenerator(std::string const & pathInRegionsTmpMwm, std::string const & pathInRegionsCollector,
                   std::string const & pathOutRegionsKv, std::string const & pathOutRepackedRegionsTmpMwm,
                   bool verbose, size_t threadsCount)
    : pathInRegionsTmpMwm{pathInRegionsTmpMwm}
    , regionsKv{pathOutRegionsKv, std::ofstream::out}
    , featuresCollector{pathOutRepackedRegionsTmpMwm}
    , verbose{verbose}
  {
    LOG(LINFO, ("Start generating regions from ", pathInRegionsTmpMwm));
    auto timer = base::Timer();
    Transliteration::Instance().Init(GetPlatform().ResourcesDir());

    RegionInfo regionsInfoCollector(pathInRegionsCollector);
    RegionsBuilder::Regions regions;
    RegionsBuilder::PlaceCentersMap placeCentersMap;
    std::tie(regions, placeCentersMap) = ReadDatasetFromTmpMwm(pathInRegionsTmpMwm, regionsInfoCollector);
    RegionsBuilder builder(std::move(regions), std::move(placeCentersMap), threadsCount);
    GenerateRegions(builder);

    LOG(LINFO, ("Regions objects key-value for", builder.GetCountryNames().size(),
                "countries storage saved to",  pathOutRegionsKv));
    LOG(LINFO, ("Repacked regions temprory mwm saved to",  pathOutRepackedRegionsTmpMwm));
    LOG(LINFO, (regionsTotalCount, "total ids.", countriesRegionIds.size(), "unique ids."));
    LOG(LINFO, ("Finish generating regions.", timer.ElapsedSeconds(), "seconds."));
  }

  void GenerateRegions(RegionsBuilder & builder)
  {
    builder.ForEachNormalizedCountry([&, this](std::string const & name, Node::Ptr const & tree) {
      if (!tree)
        return;

      if (verbose)
        DebugPrintTree(tree);

      GenerateCountry(name, tree);
    });
  }

  void GenerateCountry(std::string const & name, Node::Ptr const & tree)
  {
    LOG(LINFO, ("Processing country", name));

    std::multimap<base::GeoObjectId, Node::Ptr> countryRegions;
    GenerateKv(name, tree, countryRegions);

    // todo(maksimandrianov1): Perhaps this is not the best solution. This is a hot fix. Perhaps it
    // is better to transfer this to index generation(function GenerateRegionsData),
    // or to combine index generation and key-value storage generation in
    // generator_tool(generator_tool.cpp).
    RepackTmpMwm(pathInRegionsTmpMwm, countryRegions);
  }

  void GenerateKv(std::string const & name, Node::Ptr const & tree,
                  std::multimap<base::GeoObjectId, Node::Ptr> & countryRegions)
  {
    size_t countryRegionsCount = 0;
    size_t countryRegionObjectCount = 0;
    auto country = std::make_shared<std::string>(name);

    auto jsonPolicy = std::make_unique<JsonPolicy>(verbose);
    ForEachLevelPath(tree, [&] (std::vector<Node::Ptr> const & path) {
      ++regionsTotalCount;
      ++countryRegionsCount;

      auto const & node = path.back();
      auto const & place = node->GetData();
      auto const & placeId = place.GetId();
      auto && regionEmplace = countriesRegionIds.emplace(placeId, country);
      if (!regionEmplace.second)
      {
        if (regionEmplace.first->second != country) // object may have several regions
        {
          LOG(LWARNING, ("Failed to place region", placeId, "(", place.GetName(), ")",
                         "into", name, ": region exist in", *regionEmplace.first->second,
                         "already"));
        }
        return;
      }

      regionsKv << static_cast<int64_t>(placeId.GetEncodedId()) << " " << jsonPolicy->ToString(path) << "\n";
      countryRegions.emplace(placeId, node);
      ++countryRegionObjectCount;
    });

    LOG(LINFO, ("Country regions of", name, "has built: ", countryRegionsCount, "total ids.",
                countryRegionObjectCount, "object ids."));
  }

  std::tuple<RegionsBuilder::Regions, RegionsBuilder::PlaceCentersMap>
  ReadDatasetFromTmpMwm(std::string const & tmpMwmFilename, RegionInfo & collector)
  {
    RegionsBuilder::Regions regions;
    RegionsBuilder::PlaceCentersMap placeCentersMap;
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
        placeCentersMap.emplace(id, PlaceCenter{fb, collector.Get(id)});
      }
    };

    feature::ForEachFromDatRawFormat(tmpMwmFilename, toDo);
    return std::make_tuple(std::move(regions), std::move(placeCentersMap));
  }

  void RepackTmpMwm(std::string const & srcFilename,
                    std::multimap<base::GeoObjectId, Node::Ptr> const & countryRegions)
  {
    std::set<base::GeoObjectId> processedObjects;
    auto const toDo = [&](FeatureBuilder1 & fb, uint64_t /* currPos */) {
      auto id = fb.GetMostGenericOsmId();
      auto objectRegions = countryRegions.equal_range(id);
      if (objectRegions.first == objectRegions.second)
        return;

      if (!processedObjects.insert(id).second)
        return;

      for (auto region = objectRegions.first; region != objectRegions.second; ++region)
      {
        ResetGeometry(fb, region->second->GetData().GetRegion());
        fb.SetOsmId(id);
        fb.SetRank(0);
        featuresCollector(fb);
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
