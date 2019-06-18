#include "generator/final_processor_intermediate_mwm.hpp"

#include "generator/affiliation.hpp"
#include "generator/booking_dataset.hpp"
#include "generator/city_boundary_processor.hpp"
#include "generator/feature_builder.hpp"
#include "generator/feature_merger.hpp"
#include "generator/type_helper.hpp"

#include "indexer/classificator.hpp"

#include "platform/platform.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"
#include "base/string_utils.hpp"
#include "base/thread_pool_computational.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <future>
#include <iterator>
#include <memory>
#include <unordered_set>
#include <vector>

#include "defines.hpp"

using namespace base::thread_pool;
using namespace feature;

namespace generator
{
namespace
{
template <typename ToDo>
void ForEachCountry(std::string const & temproryMwmPath, ToDo && toDo)
{
  Platform::FilesList fileList;
  Platform::GetFilesByExt(temproryMwmPath, DATA_FILE_EXTENSION_TMP, fileList);
  for (auto const & filename : fileList)
    toDo(filename);
}

std::vector<std::vector<std::string>>
GetAffilations(std::vector<FeatureBuilder> const & fbs,
               CountriesFilesAffiliation const & affilation, size_t threadsCount)
{
  computational::ThreadPool pool(threadsCount);
  std::vector<std::future<std::vector<std::string>>> futuresAffilations;
  for (auto const & fb : fbs)
  {
    auto result = pool.Submit([&]() {
      return affilation.GetAffiliations(fb);
    });
    futuresAffilations.emplace_back(std::move(result));
  }
  std::vector<std::vector<std::string>> resultAffilations;
  resultAffilations.reserve(futuresAffilations.size());
  for (auto & f : futuresAffilations)
    resultAffilations.emplace_back(f.get());
  return resultAffilations;
}

std::vector<std::vector<std::string>>
AppendFeaturesToCountries(std::string const & temproryMwmPath,
                          std::vector<FeatureBuilder> const & fbs,
                          CountriesFilesAffiliation const & affilation,
                          size_t threadsCount)
{

  auto affilations = GetAffilations(fbs, affilation, threadsCount);
  std::unordered_map<std::string, std::vector<FeatureBuilder>> countryToCities;
  for (size_t i = 0; i < fbs.size(); ++i)
  {
    for (auto const & country : affilations[i])
      countryToCities[country].emplace_back(fbs[i]);
  }
  delayed::ThreadPool pool(threadsCount, delayed::ThreadPool::Exit::ExecPending);
  for (auto & p : countryToCities)
  {
    pool.Push([temproryMwmPath, name{p.first}, countries{std::move(p.second)}]() {
      auto const path = base::JoinPath(temproryMwmPath, name + DATA_FILE_EXTENSION_TMP);
      FeaturesCollector collector(path, FileWriter::Op::OP_APPEND);
      for (auto && fb : countries)
        collector.Collect(std::move(fb));
    });
  }
  return affilations;
}

bool FilenameIsCountry(std::string filename, CountriesFilesAffiliation const & affilation)
{
  strings::ReplaceFirst(filename, DATA_FILE_EXTENSION_TMP, "");
  return !affilation.HasRegionByName(filename);
}

class CityBoundariesHelper
{
public:
  explicit CityBoundariesHelper(std::string const & filename)
    : m_table(std::make_shared<OsmIdToBoundariesTable>())
    , m_processor(m_table)
  {
    ForEachFromDatRawFormat(filename, [&](auto const & fb, auto const &) {
      m_processor.Add(fb);
    });
  }

  static bool IsPlace(FeatureBuilder const & fb)
  {
    auto const type = GetPlaceType(fb);
    return type != ftype::GetEmptyValue();
  }

  bool Process(FeatureBuilder const & fb)
  {
    if (IsPlace(fb) && !fb.GetName().empty())
    {
      m_processor.Replace(fb);
      return true;
    }

    return false;
  }

  std::vector<FeatureBuilder> GetFeatures() const
  {
    return m_processor.GetFeatures();
  }

  std::shared_ptr<OsmIdToBoundariesTable> GetTable() const
  {
    return m_table;
  }

private:
  std::shared_ptr<OsmIdToBoundariesTable> m_table;
  CityBoundaryProcessor m_processor;
};
}  // namespace

bool FinalProcessorIntermediateMwmInteface::operator<(FinalProcessorIntermediateMwmInteface const & other) const
{
  return m_priority < other.m_priority;
}

bool FinalProcessorIntermediateMwmInteface::operator==(FinalProcessorIntermediateMwmInteface const & other) const
{
  return *this < other && other < *this;
}

bool FinalProcessorIntermediateMwmInteface::operator!=(FinalProcessorIntermediateMwmInteface const & other) const
{
  return *this < other || other < *this;
}

CountryFinalProcessor::CountryFinalProcessor(std::string const & borderPath,
                                             std::string const & temproryMwmPath,
                                             size_t threadsCount)
  : FinalProcessorIntermediateMwmInteface(FinalProcessorPriority::COUNTRIES_WORLD)
  , m_borderPath(borderPath)
  , m_temproryMwmPath(temproryMwmPath)
  , m_threadsCount(threadsCount)
{
}

void CountryFinalProcessor::NeedBookig(std::string const & filename)
{
  m_hotelsPath = filename;
}

void CountryFinalProcessor::UseCityBoundaries(std::string const & filename)
{
  m_cityBoundariesTmpFilename = filename;
}

void CountryFinalProcessor::AddCoastlines(std::string const & coastlineGeomFilename, std::string const & worldCoastsFilename)
{
  m_coastlineGeomFilename = coastlineGeomFilename;
  m_worldCoastsFilename = worldCoastsFilename;
}

bool CountryFinalProcessor::Process()
{
  if (!m_hotelsPath.empty() && !ProcessBooking())
    return false;

  if (!m_cityBoundariesTmpFilename.empty() && !ProcessCityBoundaries())
    return false;

  if (!m_coastlineGeomFilename.empty() && !ProcessCoasline())
    return false;


  return CleanUp();
}

bool CountryFinalProcessor::ProcessBooking()
{
  BookingDataset dataset(m_hotelsPath);
  auto const affilation = CountriesFilesAffiliation(m_borderPath);
  {
    delayed::ThreadPool pool(m_threadsCount, delayed::ThreadPool::Exit::ExecPending);
    ForEachCountry(m_temproryMwmPath, [&](auto const & filename) {
      pool.Push([&, filename]() {
        std::vector<FeatureBuilder> cities;
        if (!FilenameIsCountry(filename, affilation))
          return;
        auto const fullPath = base::JoinPath(m_temproryMwmPath, filename);
        auto fbs = ReadAllDatRawFormat(fullPath);
        FeaturesCollector collector(fullPath);
        for (auto & fb : fbs)
        {
          auto const id = dataset.FindMatchingObjectId(fb);
          if (id == BookingHotel::InvalidObjectId())
          {
            collector.Collect(fb);
          }
          else
          {
            dataset.PreprocessMatchedOsmObject(id, fb, [&](FeatureBuilder & newFeature) {
              collector.Collect(newFeature);
            });
          }
        }
      });
    });
  }
  std::vector<FeatureBuilder> fbs;
  dataset.BuildOsmObjects([&](auto && fb) {
    fbs.emplace_back(std::move(fb));
  });
  AppendFeaturesToCountries(m_temproryMwmPath, std::move(fbs), affilation, m_threadsCount);
  return true;
}

bool CountryFinalProcessor::ProcessCityBoundaries()
{
  auto const affilation = CountriesFilesAffiliation(m_borderPath);
  std::vector<std::future<std::vector<FeatureBuilder>>> citiesResults;
  computational::ThreadPool pool(m_threadsCount);
  ForEachCountry(m_temproryMwmPath, [&](auto const & filename) {
    auto cities = pool.Submit([&, filename]() {
      std::vector<FeatureBuilder> cities;
      if (!FilenameIsCountry(filename, affilation))
        return cities;
      auto const fullPath = base::JoinPath(m_temproryMwmPath, filename);
      auto fbs = ReadAllDatRawFormat(fullPath);
      FeaturesCollector collector(fullPath);
      for (size_t i = 0; i < fbs.size(); ++i)
      {
        if (CityBoundariesHelper::IsPlace(fbs[i]))
          cities.emplace_back(std::move(fbs[i]));
        else
          collector.Collect(std::move(fbs[i]));
      }
      return cities;
    });
    citiesResults.emplace_back(std::move(cities));
  });
  CityBoundariesHelper cityBoundariesHelper(m_cityBoundariesTmpFilename);
  for (auto & v : citiesResults)
  {
    auto const cities = v.get();
    for (auto const & city : cities)
      cityBoundariesHelper.Process(city);
  }
  auto const fbs = cityBoundariesHelper.GetFeatures();
  AppendFeaturesToCountries(m_temproryMwmPath, fbs, affilation, m_threadsCount);
  return true;
}

bool CountryFinalProcessor::ProcessCoasline()
{
  auto const affilation = CountriesFilesAffiliation(m_borderPath);
  auto fbs = ReadAllDatRawFormat(m_coastlineGeomFilename);
  auto const affilations = AppendFeaturesToCountries(m_temproryMwmPath, fbs, affilation, m_threadsCount);
  FeaturesCollector collector(m_worldCoastsFilename);
  for (size_t i = 0; i < fbs.size(); ++i)
  {
    fbs[i].AddName("default", strings::JoinStrings(affilations[i], ";"));
    collector.Collect(fbs[i]);
  }
  return true;
}

bool CountryFinalProcessor::CleanUp()
{
  auto const affilation = CountriesFilesAffiliation(m_borderPath);
  delayed::ThreadPool pool(m_threadsCount, delayed::ThreadPool::Exit::ExecPending);
  ForEachCountry(m_temproryMwmPath, [&](auto const & filename) {
    pool.Push([&, filename]() {
      if (!FilenameIsCountry(filename, affilation))
        return;
      auto const fullPath = base::JoinPath(m_temproryMwmPath, filename);
      auto fbs = ReadAllDatRawFormat(fullPath);
      FeaturesCollector collector(fullPath);
      for (auto & fb : fbs)
      {
        if (fb.RemoveInvalidTypes() && PreprocessForCountryMap(fb))
          collector.Collect(fb);
      }
    });
  });
  return true;
}

WorldFinalProcessor::WorldFinalProcessor(std::string const & worldTmpFilename,
                                         std::string const & coastlineGeomFilename,
                                         std::string const & popularPlacesFilename)
  : FinalProcessorIntermediateMwmInteface(FinalProcessorPriority::COUNTRIES_WORLD)
  , m_worldTmpFilename(worldTmpFilename)
  , m_coastlineGeomFilename(coastlineGeomFilename)
  , m_popularPlacesFilename(popularPlacesFilename)
{
}

void WorldFinalProcessor::UseCityBoundaries(std::string const & filename)
{
  m_cityBoundariesTmpFilename = filename;
}

bool WorldFinalProcessor::Process()
{
  if (!m_cityBoundariesTmpFilename.empty() && !ProcessCityBoundaries())
    return false;
  auto fbs = ReadAllDatRawFormat(m_worldTmpFilename);
  WorldGenerator generator(m_worldTmpFilename, m_coastlineGeomFilename, m_popularPlacesFilename);
  for (auto & fb : fbs)
  {
    if (fb.RemoveInvalidTypes())
      generator.Process(fb);
  }
  generator.DoMerge();
  return true;
}

bool WorldFinalProcessor::ProcessCityBoundaries()
{
  auto fbs = ReadAllDatRawFormat(m_worldTmpFilename);
  CityBoundariesHelper cityBoundariesHelper(m_cityBoundariesTmpFilename);
  std::unordered_set<size_t> indexes;
  for (size_t i = 0; i < fbs.size(); ++i)
  {
    auto const & fb = fbs[i];
    if (CityBoundariesHelper::IsPlace(fb))
    {
      cityBoundariesHelper.Process(fb);
      indexes.emplace(i);
    }
  }
  auto cities = cityBoundariesHelper.GetFeatures();
  std::move(std::begin(cities), std::end(cities), std::back_inserter(fbs));
  FeaturesCollector collector(m_worldTmpFilename);
  for (size_t i = 0; i < fbs.size(); ++i)
  {
    if (indexes.count(i) == 0)
      collector.Collect(fbs[i]);
  }
  return true;
}

CoastlineFinalProcessor::CoastlineFinalProcessor(std::string const & filename)
  : FinalProcessorIntermediateMwmInteface(FinalProcessorPriority::WORLDCOASTS)
  , m_filename(filename)
{
}

void CoastlineFinalProcessor::SetCoastlineGeomFilename(std::string const & filename)
{
  m_coastlineGeomFilename = filename;
}

void CoastlineFinalProcessor::SetCoastlineRawGeomFilename(std::string const & filename)
{
  m_coastlineRawGeomFilename = filename;
}

bool CoastlineFinalProcessor::Process()
{
  ForEachFromDatRawFormat(m_filename, [&](auto && fb, auto const &) {
    m_generator.Process(std::move(fb));
  });
  FeaturesAndRawGeometryCollector collector(m_coastlineGeomFilename, m_coastlineRawGeomFilename);
  // Check and stop if some coasts were not merged.
  if (!m_generator.Finish())
    return false;
  LOG(LINFO, ("Generating coastline polygons."));
  size_t totalFeatures = 0;
  size_t totalPoints = 0;
  size_t totalPolygons = 0;
  vector<FeatureBuilder> features;
  m_generator.GetFeatures(features);
  for (auto & feature : features)
  {
    collector.Collect(feature);
    ++totalFeatures;
    totalPoints += feature.GetPointsCount();
    totalPolygons += feature.GetPolygonsCount();
  }
  LOG(LINFO, ("Total features:", totalFeatures, "total polygons:", totalPolygons,
              "total points:", totalPoints));
  return true;
}
}  // namespace generator
