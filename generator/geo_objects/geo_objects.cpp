#include "generator/geo_objects/geo_objects.hpp"

#include "generator/feature_builder.hpp"
#include "generator/locality_sorter.hpp"
#include "generator/regions/region_base.hpp"

#include "indexer/borders.hpp"
#include "indexer/classificator.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/locality_index.hpp"
#include "indexer/locality_index_builder.hpp"

#include "coding/mmap_reader.hpp"

#include "geometry/mercator.hpp"

#include "base/async.hpp"
#include "base/geo_object_id.hpp"
#include "base/logging.hpp"
#include "base/thread_pool_computational.hpp"
#include "base/thread_batch.hpp"
#include "base/timer.hpp"

#include <cstdint>
#include <fstream>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

#include "platform/platform.hpp"

#include <boost/optional.hpp>
#include "3party/jansson/myjansson.hpp"

using namespace base::threads;
using namespace base::thread_pool::computational;

namespace
{
using KeyValue = std::pair<uint64_t, base::Json>;
using IndexReader = ReaderPtr<Reader>;

bool DefaultPred(KeyValue const &) { return true; }

bool ParseKey(std::string const & line, int64_t & key)
{
  auto const pos = line.find(" ");
  if (pos == std::string::npos)
  {
    LOG(LWARNING, ("Cannot find separator."));
    return false;
  }

  if (!strings::to_int64(line.substr(0, pos), key))
  {
    LOG(LWARNING, ("Cannot parse id."));
    return false;
  }

  return true;
}

bool ParseKeyValueLine(std::string const & line, KeyValue & res)
{
  auto const pos = line.find(" ");
  if (pos == std::string::npos)
  {
    LOG(LWARNING, ("Cannot find separator."));
    return false;
  }

  int64_t id;
  if (!strings::to_int64(line.substr(0, pos), id))
  {
    LOG(LWARNING, ("Cannot parse id."));
    return false;
  }

  base::Json json;
  try
  {
    json = base::Json(line.substr(pos + 1));
    if (!json.get())
      return false;
  }
  catch (base::Json::Exception const &)
  {
    LOG(LWARNING, ("Cannot create base::Json."));
    return false;
  }

  res = std::make_pair(static_cast<uint64_t>(id), json);
  return true;
}

// An interface for reading key-value storage.
class KeyValueInterface
{
public:
  virtual ~KeyValueInterface() = default;

  virtual boost::optional<base::Json> Find(uint64_t key) const = 0;
  virtual size_t Size() const = 0;
};

// An implementation for reading key-value storage with loading and searching in memory.
class KeyValueMem : public KeyValueInterface
{
public:
  KeyValueMem(std::istream & stream, std::function<bool(KeyValue const &)> pred = DefaultPred)
  {
    std::string line;
    KeyValue kv;
    while (std::getline(stream, line))
    {
      if (!ParseKeyValueLine(line, kv) || !pred(kv))
        continue;

      m_map.insert(kv);
    }
  }

  // KeyValueInterface overrides:
  boost::optional<base::Json> Find(uint64_t key) const override
  {
    auto const it = m_map.find(key);
    if (it == std::end(m_map))
      return {};

    auto inWorker = m_ownerId != std::this_thread::get_id();
    if (inWorker)
      return it->second.GetDeepCopy(); // for thread safety

    return it->second;
  }

  size_t Size() const override { return m_map.size(); }

private:
  std::unordered_map<uint64_t, base::Json> m_map;
  std::thread::id m_ownerId = std::this_thread::get_id();
};

// An implementation for reading key-value storage with loading and searching in disk.
class KeyValueMap : public KeyValueInterface
{
public:
  KeyValueMap(std::istream & stream) : m_stream(stream)
  {
    std::string line;
    std::istream::pos_type pos = 0;
    KeyValue kv;
    while (std::getline(m_stream, line))
    {
      int64_t key;
      if (!ParseKey(line, key))
        continue;

      m_map.emplace(key, pos);
      pos = m_stream.tellg();
    }

    m_stream.clear();
  }

  // KeyValueInterface overrides:
  boost::optional<base::Json> Find(uint64_t key) const override
  {
    auto const it = m_map.find(key);
    if (it == std::end(m_map))
      return {};

    std::string line;
    if (!ReadLine(it->second, line))
    {
      LOG(LERROR, ("Cannot read KV-line for", key, "at position", it->second));
      return {};
    }

    if (line.empty())
    {
      LOG(LERROR, ("Empty KV-line for", key, "at position", it->second));
      return {};
    }

    KeyValue kv;
    if (!ParseKeyValueLine(line, kv))
      return {};

    return std::move(kv.second);
  }

  size_t Size() const override { return m_map.size(); }

private:
  bool ReadLine(std::istream::pos_type pos, std::string & line) const
  {
    std::unique_lock<std::mutex> lock(m_mutex, std::defer_lock);
    auto inWorker = m_ownerId != std::this_thread::get_id();
    if (inWorker)
      lock.lock();

    m_stream.seekg(pos);
    if (!std::getline(m_stream, line))
      return false;

    return true;
  }

  std::istream & m_stream;
  std::unordered_map<uint64_t, std::istream::pos_type> m_map;
  mutable std::mutex m_mutex;
  std::thread::id m_ownerId = std::this_thread::get_id();
};

bool IsBuilding(FeatureBuilder1 const & fb)
{
  auto const & checker = ftypes::IsBuildingChecker::Instance();
  return checker(fb.GetTypes());
}

bool HasHouse(FeatureBuilder1 const & fb)
{
  return !fb.GetParams().house.IsEmpty();
}

bool HouseHasAddress(base::Json json)
{
  auto properties = json_object_get(json.get(), "properties");
  auto address = json_object_get(properties, "address");
  std::string const kHouseField = "building";
  char const * key = nullptr;
  json_t * value = nullptr;
  json_object_foreach(address, key, value)
  {
    if (key == kHouseField && !json_is_null(value))
      return true;
  }

  return false;
}

template <typename Index>
std::vector<base::GeoObjectId> SearchObjectsInIndex(FeatureBuilder1 const & fb, Index const & index)
{
  std::vector<base::GeoObjectId> ids;
  auto const fn = [&ids] (base::GeoObjectId const & osmid) { ids.emplace_back(osmid); };
  auto const center = fb.GetKeyPoint();
  index.ForEachInRect(fn, m2::RectD(center, center));
  return ids;
}

int GetRankFromValue(base::Json json)
{
  int rank;
  auto properties = json_object_get(json.get(), "properties");
  FromJSONObject(properties, "rank", rank);
  return rank;
}

boost::optional<KeyValue> GetDeepestRegionInBorder(FeatureBuilder1 const & fb,
    std::vector<base::GeoObjectId> const & ids, indexer::Borders const & regionBorders,
    KeyValueInterface const & regionKv)
{
  // Minimize CPU consume by minimize calls of havy regionBorders.IsPointInside().
  std::multimap<int, KeyValue> regionsByRank;
  auto const center = fb.GetKeyPoint();
  for (auto const & id : ids)
  {
    base::Json temp;
    auto const res = regionKv.Find(id.GetEncodedId());
    if (!res)
    {
      LOG(LWARNING, ("Id not found in region key-value storage:", id));
      continue;
    }

    if (!json_is_object((*res).get()))
    {
      LOG(LWARNING, ("Value is not a json object in region key-value storage:", id));
      continue;
    }

    auto rank = GetRankFromValue(*res);
    auto idNumeric = static_cast<int64_t>(id.GetEncodedId());
    regionsByRank.emplace(rank, KeyValue{idNumeric, std::move(*res)});
  }

  for (auto i = regionsByRank.rbegin(); i != regionsByRank.rend(); ++i)
  {
    auto & kv = i->second;
    if (regionBorders.IsPointInside(kv.first, center))
      return std::move(kv);
  }

  return {};
}

void UpdateCoordinates(m2::PointD const & point, base::Json json)
{
  auto geometry = json_object_get(json.get(), "geometry");
  auto coordinates = json_object_get(geometry, "coordinates");
  if (json_array_size(coordinates) == 2)
  {
    auto const latLon = MercatorBounds::ToLatLon(point);
    json_array_set_new(coordinates, 0, ToJSON(latLon.lat).release());
    json_array_set_new(coordinates, 1, ToJSON(latLon.lon).release());
  }
}

base::Json AddAddress(FeatureBuilder1 const & fb, KeyValue const & regionKeyValue)
{
  base::Json result = regionKeyValue.second.GetDeepCopy();
  int const kHouseOrPoiRank = 30;
  UpdateCoordinates(fb.GetKeyPoint(), result);
  auto properties = json_object_get(result.get(), "properties");
  auto address = json_object_get(properties, "address");
  ToJSONObject(*properties, "rank", kHouseOrPoiRank);
  auto const street = fb.GetParams().GetStreet();
  if (!street.empty())
    ToJSONObject(*address, "street", street);

  // By writing home null in the field we can understand that the house has no address.
  auto const house = fb.GetParams().house.Get();
  if (!house.empty())
    ToJSONObject(*address, "building", house);
  else
    ToJSONObject(*address, "building", base::NewJSONNull());

  ToJSONObject(*properties, "pid", regionKeyValue.first);
  // auto locales = json_object_get(result.get(), "locales");
  // auto en = json_object_get(result.get(), "en");
  // todo(maksimandrianov): Add en locales.
  return result;
}

boost::optional<KeyValue>
FindRegion(FeatureBuilder1 const & fb, indexer::RegionsIndex<IndexReader> const & regionIndex,
           indexer::Borders const & regionBorders, KeyValueInterface const & regionKv)
{
  auto const ids = SearchObjectsInIndex(fb, regionIndex);
  return GetDeepestRegionInBorder(fb, ids, regionBorders, regionKv);
}

std::unique_ptr<char, JSONFreeDeleter>
MakeGeoObjectValueWithAddress(FeatureBuilder1 const & fb, KeyValue const & keyValue)
{
  auto const jsonWithAddress = AddAddress(fb, keyValue);
  auto const cstr = json_dumps(jsonWithAddress.get(), JSON_COMPACT);
  return std::unique_ptr<char, JSONFreeDeleter>(cstr);
}

boost::optional<base::Json>
FindHousePoi(FeatureBuilder1 const & fb,
             indexer::GeoObjectsIndex<IndexReader> const & geoObjectsIndex,
             KeyValueInterface const & geoObjectsKv)
{
  auto const ids = SearchObjectsInIndex(fb, geoObjectsIndex);
  for (auto const & id : ids)
  {
    auto const house = geoObjectsKv.Find(id.GetEncodedId());
    if (!house)
      continue;

    auto properties = json_object_get(house->get(), "properties");
    auto address = json_object_get(properties, "address");
    if (json_object_get(address, "building"))
      return house;
  }

  return {};
}

std::unique_ptr<char, JSONFreeDeleter>
MakeGeoObjectValueWithoutAddress(FeatureBuilder1 const & fb, base::Json json)
{
  auto const jsonWithAddress = json.GetDeepCopy();
  auto properties = json_object_get(jsonWithAddress.get(), "properties");
  ToJSONObject(*properties, "name", fb.GetName());
  UpdateCoordinates(fb.GetKeyPoint(), jsonWithAddress);
  auto const cstr = json_dumps(jsonWithAddress.get(), JSON_COMPACT);
  return std::unique_ptr<char, JSONFreeDeleter>(cstr);
}

boost::optional<indexer::GeoObjectsIndex<IndexReader>>
MakeTempGeoObjectsIndex(std::string const & pathToGeoObjectsTmpMwm)
{
  auto const dataFile = Platform().TmpPathForFile();
  SCOPE_GUARD(removeDataFile, std::bind(Platform::RemoveFileIfExists, std::cref(dataFile)));
  if (!feature::GenerateGeoObjectsData(pathToGeoObjectsTmpMwm, "" /* nodesFile */, dataFile))
  {
    LOG(LCRITICAL, ("Error generating geo objects data."));
    return {};
  }

  auto const indexFile = Platform().TmpPathForFile();
  SCOPE_GUARD(removeIndexFile, std::bind(Platform::RemoveFileIfExists, std::cref(indexFile)));
  if (!indexer::BuildGeoObjectsIndexFromDataFile(dataFile, indexFile))
  {
    LOG(LCRITICAL, ("Error generating geo objects index."));
    return {};
  }

  return indexer::ReadIndex<indexer::GeoObjectsIndexBox<IndexReader>, MmapReader>(indexFile);
}

void BuildGeoObjectsWithAddresses(indexer::RegionsIndex<IndexReader> const & regionIndex,
                                  indexer::Borders const & regionBorders,
                                  KeyValueInterface const & regionKv,
                                  std::string const & pathInGeoObjectsTmpMwm,
                                  std::ostream & streamGeoObjectsKv,
                                  bool /* verbose */,
                                  size_t threadsCount)
{
  size_t countGeoObjects = 0;

  ThreadPool filterThreadPool{threadsCount};
  base::threads::AsyncFinishFlag completeFlag;
  std::mutex streamGeoObjectMutex;

  {
    constexpr size_t kFilterBatchSize = 1000;
    BatchSubmitter filterSubmitter{filterThreadPool, kFilterBatchSize}; // in scope for auto flush

    auto const fn = [&](FeatureBuilder1 & fb, uint64_t /* currPos */) {
      if (!(IsBuilding(fb) || HasHouse(fb)))
        return;

      filterSubmitter([&, fb, completeLock = completeFlag.GetLock()] {
        auto regionKeyValue = FindRegion(fb, regionIndex, regionBorders, regionKv);
        if (!regionKeyValue)
          return;

        auto const value = MakeGeoObjectValueWithAddress(fb, *regionKeyValue);
        {
          std::lock_guard<std::mutex> lock(streamGeoObjectMutex);
          streamGeoObjectsKv << static_cast<int64_t>(fb.GetMostGenericOsmId().GetEncodedId()) << " "
                           << value.get() << "\n";
          ++countGeoObjects;
        }
      });
    };

    feature::ForEachFromDatRawFormat(pathInGeoObjectsTmpMwm, fn);
  }

  completeFlag.Wait();
  LOG(LINFO, ("Added ", countGeoObjects, "geo objects with addresses."));
}

void BuildGeoObjectsWithoutAddresses(indexer::GeoObjectsIndex<IndexReader> const & geoObjectsIndex,
                                     std::string const & pathInGeoObjectsTmpMwm,
                                     KeyValueInterface const & geoObjectsKv,
                                     std::ostream & streamGeoObjectsKv,
                                     std::ostream & streamIdsWithoutAddress, bool)
{
  size_t countGeoObjects = 0;
  auto const fn  = [&](FeatureBuilder1 & fb, uint64_t /* currPos */) {
    if (IsBuilding(fb) || HasHouse(fb))
      return;

    auto const house = FindHousePoi(fb, geoObjectsIndex, geoObjectsKv);
    if (!house)
      return;

    if (!HouseHasAddress(*house))
      return;

    auto const value = MakeGeoObjectValueWithoutAddress(fb, *house);
    auto const id = static_cast<int64_t>(fb.GetMostGenericOsmId().GetEncodedId());
    streamGeoObjectsKv << id << " " << value.get() << "\n";
    streamIdsWithoutAddress << id << "\n";
    ++countGeoObjects;
  };

  feature::ForEachFromDatRawFormat(pathInGeoObjectsTmpMwm, fn);
  LOG(LINFO, ("Added ", countGeoObjects, "geo objects without addresses."));
}
}  // namespace

namespace generator
{
namespace geo_objects
{
bool GenerateGeoObjects(std::string const & pathInRegionsIndex,
                        std::string const & pathInRegionsKv,
                        std::string const & pathInGeoObjectsTmpMwm,
                        std::string const & pathOutIdsWithoutAddress,
                        std::string const & pathOutGeoObjectsKv,
                        bool verbose,
                        size_t threadsCount)
{
  LOG(LINFO, ("Start generating geo objects.."));
  auto timer = base::Timer();
  SCOPE_GUARD(finishGeneratingGeoObjects, [&timer]() {
    LOG(LINFO, ("Finish generating geo objects.", timer.ElapsedSeconds(), "seconds."));
  });

  auto geoObjectIndexFuture = std::async(std::launch::async, MakeTempGeoObjectsIndex,
                                         pathInGeoObjectsTmpMwm);
  auto const regionIndex =
      indexer::ReadIndex<indexer::RegionsIndexBox<IndexReader>, MmapReader>(pathInRegionsIndex);
  indexer::Borders regionBorders;
  regionBorders.Deserialize(pathInRegionsIndex);
  // Regions key-value storage is small (~150 Mb). We will load everything into memory.
  std::fstream streamRegionKv(pathInRegionsKv);
  KeyValueMem const regionsKv(streamRegionKv);
  LOG(LINFO, ("Size of regions key-value storage:", regionsKv.Size()));
  std::ofstream streamIdsWithoutAddress(pathOutIdsWithoutAddress);
  std::ofstream streamGeoObjectsKv(pathOutGeoObjectsKv);
  BuildGeoObjectsWithAddresses(regionIndex, regionBorders, regionsKv, pathInGeoObjectsTmpMwm,
                               streamGeoObjectsKv, verbose, threadsCount);
  LOG(LINFO, ("Geo objects with addresses were built."));
  // Regions key-value storage is big (~80 Gb). We will not load the key value into memory.
  // This can be slow.
  // todo(maksimandrianov1): Investigate the issue of performance and if necessary improve.
  std::ifstream tempStream(pathOutGeoObjectsKv);
  auto const pred = [](KeyValue const & kv) { return HouseHasAddress(kv.second); };
  KeyValueMem const geoObjectsKv(tempStream, pred);
  LOG(LINFO, ("Size of geo objects key-value storage:", geoObjectsKv.Size()));
  auto const geoObjectIndex = geoObjectIndexFuture.get();
  LOG(LINFO, ("Index was built."));
  if (!geoObjectIndex)
    return false;

  BuildGeoObjectsWithoutAddresses(*geoObjectIndex, pathInGeoObjectsTmpMwm, geoObjectsKv,
                                  streamGeoObjectsKv, streamIdsWithoutAddress, verbose);
  LOG(LINFO, ("Geo objects without addresses were built."));
  LOG(LINFO, ("Geo objects key-value storage saved to",  pathOutGeoObjectsKv));
  LOG(LINFO, ("Ids of POIs without addresses saved to", pathOutIdsWithoutAddress));
  return true;
}
}  // namespace geo_objects
}  // namespace generator
