#include "routing/index_graph_loader.hpp"

#include "routing/city_roads.hpp"
#include "routing/index_graph_serialization.hpp"
#include "routing/restriction_loader.hpp"
#include "routing/road_access.hpp"
#include "routing/road_access_serialization.hpp"
#include "routing/route.hpp"
#include "routing/routing_exceptions.hpp"
#include "routing/speed_camera_ser_des.hpp"

#include "indexer/data_source.hpp"

#include "platform/country_defines.hpp"

#include "coding/files_container.hpp"

#include "base/assert.hpp"
#include "base/optional_lock_guard.hpp"
#include "base/timer.hpp"

#include <algorithm>
#include <map>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>

namespace
{
using namespace routing;
using namespace std;

class IndexGraphLoaderImpl final : public IndexGraphLoader
{
public:
  IndexGraphLoaderImpl(VehicleType vehicleType, bool loadAltitudes, bool isTwoThreadsReady,
                       shared_ptr<NumMwmIds> numMwmIds,
                       shared_ptr<VehicleModelFactoryInterface> vehicleModelFactory,
                       shared_ptr<EdgeEstimator> estimator, DataSource & dataSource,
                       RoutingOptions routingOptions = RoutingOptions());

  /// |GetGeometry()| and |GetIndexGraph()| return a references to items in container |m_graphs|.
  /// They may be called from different threads in case of two thread bidirectional A*.
  /// The references they return are not constant but it's ok. The code should works with them
  /// taking into account |isOutgoing| parameter. On the other hand these methods may add items to
  /// |m_graphs| under a mutex. So it's possible that while one thread is working with a reference
  /// returned by |GetGeometry()| or |GetIndexGraph()| the other thread is adding item to |m_graphs|
  /// and the hash table is rehashing. Everything should work correctly because according
  /// to the standard rehashing of std::unordered_map keeps references.
  // IndexGraphLoader overrides:
  Geometry & GetGeometry(NumMwmId numMwmId) override;
  IndexGraph & GetIndexGraph(NumMwmId numMwmId) override;
  vector<RouteSegment::SpeedCamera> GetSpeedCameraInfo(Segment const & segment) override;
  void Clear() override;
  bool IsTwoThreadsReady() const override;

private:
  struct GraphAttrs
  {
    shared_ptr<Geometry> m_geometry;
    unique_ptr<IndexGraph> m_indexGraph;
  };

  GraphAttrs & CreateGeometry(NumMwmId numMwmId);
  GraphAttrs & CreateIndexGraph(NumMwmId numMwmId, GraphAttrs & graph);

  VehicleType m_vehicleType;
  bool m_loadAltitudes;
  DataSource & m_dataSource;
  shared_ptr<NumMwmIds> m_numMwmIds;
  shared_ptr<VehicleModelFactoryInterface> m_vehicleModelFactory;
  shared_ptr<EdgeEstimator> m_estimator;

  optional<mutex> m_graphsMtx;
  unordered_map<NumMwmId, GraphAttrs> m_graphs;

  unordered_map<NumMwmId, map<SegmentCoord, vector<RouteSegment::SpeedCamera>>> m_cachedCameras;
  decltype(m_cachedCameras)::iterator ReceiveSpeedCamsFromMwm(NumMwmId numMwmId);

  RoutingOptions m_avoidRoutingOptions = RoutingOptions();
  std::function<time_t()> m_currentTimeGetter = [time = GetCurrentTimestamp()]() {
    return time;
  };
};

IndexGraphLoaderImpl::IndexGraphLoaderImpl(
    VehicleType vehicleType, bool loadAltitudes, bool isTwoThreadsReady, shared_ptr<NumMwmIds> numMwmIds,
    shared_ptr<VehicleModelFactoryInterface> vehicleModelFactory,
    shared_ptr<EdgeEstimator> estimator, DataSource & dataSource,
    RoutingOptions routingOptions)
  : m_vehicleType(vehicleType)
  , m_loadAltitudes(loadAltitudes)
  , m_dataSource(dataSource)
  , m_numMwmIds(move(numMwmIds))
  , m_vehicleModelFactory(move(vehicleModelFactory))
  , m_estimator(move(estimator))
  , m_graphsMtx(isTwoThreadsReady ? std::make_optional<std::mutex>() : std::nullopt)
  , m_avoidRoutingOptions(routingOptions)
{
  CHECK(m_numMwmIds, ());
  CHECK(m_vehicleModelFactory, ());
  CHECK(m_estimator, ());
}

Geometry & IndexGraphLoaderImpl::GetGeometry(NumMwmId numMwmId)
{
  base::OptionalLockGuard guard(m_graphsMtx);
  auto it = m_graphs.find(numMwmId);
  if (it != m_graphs.end())
    return *it->second.m_geometry;

  return *CreateGeometry(numMwmId).m_geometry;
}

IndexGraph & IndexGraphLoaderImpl::GetIndexGraph(NumMwmId numMwmId)
{
  base::OptionalLockGuard guard(m_graphsMtx);
  auto it = m_graphs.find(numMwmId);
  if (it != m_graphs.end())
  {
    return it->second.m_indexGraph ? *it->second.m_indexGraph
                                   : *CreateIndexGraph(numMwmId, it->second).m_indexGraph;
  }

  // Note. For the reason of putting CreateIndexGraph() under the |guard| please see
  // a detailed comment in ConvertRestrictionsOnlyUTurnToNo() method.
  return *CreateIndexGraph(numMwmId, CreateGeometry(numMwmId)).m_indexGraph;
}

auto IndexGraphLoaderImpl::ReceiveSpeedCamsFromMwm(NumMwmId numMwmId) -> decltype(m_cachedCameras)::iterator
{
  m_cachedCameras.emplace(numMwmId,
                          map<SegmentCoord, vector<RouteSegment::SpeedCamera>>{});
  auto & mapReference = m_cachedCameras.find(numMwmId)->second;

  auto const & file = m_numMwmIds->GetFile(numMwmId);
  auto handle = m_dataSource.GetMwmHandleByCountryFile(file);
  if (!handle.IsAlive())
    MYTHROW(RoutingException, ("Can't get mwm handle for", file));

  MwmValue const & mwmValue = *handle.GetValue();
  if (!mwmValue.m_cont.IsExist(CAMERAS_INFO_FILE_TAG))
  {
    LOG(LINFO, ("No section about speed cameras"));
    return m_cachedCameras.end();
  }

  try
  {
    FilesContainerR::TReader reader(mwmValue.m_cont.GetReader(CAMERAS_INFO_FILE_TAG));
    ReaderSource<FilesContainerR::TReader> src(reader);
    DeserializeSpeedCamsFromMwm(src, mapReference);
  }
  catch (Reader::OpenException & ex)
  {
    LOG(LINFO, ("No section about speed cameras"));
    return m_cachedCameras.end();
  }

  decltype(m_cachedCameras)::iterator it;
  it = m_cachedCameras.find(numMwmId);
  CHECK(it != m_cachedCameras.end(), ());

  return it;
}

vector<RouteSegment::SpeedCamera> IndexGraphLoaderImpl::GetSpeedCameraInfo(Segment const & segment)
{
  auto const numMwmId = segment.GetMwmId();

  auto it = m_cachedCameras.find(numMwmId);
  if (it == m_cachedCameras.end())
    it = ReceiveSpeedCamsFromMwm(numMwmId);

  if (it == m_cachedCameras.end())
    return {};

  auto result = it->second.find({segment.GetFeatureId(), segment.GetSegmentIdx()});
  if (result == it->second.end())
    return {};

  auto camerasTmp = result->second;
  std::sort(camerasTmp.begin(), camerasTmp.end());

  // TODO (@gmoryes) do this in generator.
  // This code matches cameras with equal coefficients and among them
  // only the camera with minimal maxSpeed is left.
  static constexpr auto kInvalidCoef = 2.0;
  camerasTmp.emplace_back(kInvalidCoef, 0.0 /* maxSpeedKmPH */);
  vector<RouteSegment::SpeedCamera> cameras;
  for (size_t i = 1; i < camerasTmp.size(); ++i)
  {
    static constexpr auto kEps = 1e-5;
    if (!base::AlmostEqualAbs(camerasTmp[i - 1].m_coef, camerasTmp[i].m_coef, kEps))
      cameras.emplace_back(camerasTmp[i - 1]);
  }
  // Cameras stored from beginning to ending of segment. So if we go at segment in backward direction,
  // we should read cameras in reverse sequence too.
  if (!segment.IsForward())
    std::reverse(cameras.begin(), cameras.end());

  return cameras;
}

IndexGraphLoaderImpl::GraphAttrs & IndexGraphLoaderImpl::CreateGeometry(NumMwmId numMwmId)
{
  platform::CountryFile const & file = m_numMwmIds->GetFile(numMwmId);
  MwmSet::MwmHandle handle = m_dataSource.GetMwmHandleByCountryFile(file);
  if (!handle.IsAlive())
    MYTHROW(RoutingException, ("Can't get mwm handle for", file));

  shared_ptr<VehicleModelInterface> vehicleModel =
      m_vehicleModelFactory->GetVehicleModelForCountry(file.GetName());

  auto & graph = m_graphs[numMwmId];
  graph.m_geometry = make_shared<Geometry>(
      GeometryLoader::Create(m_dataSource, handle, vehicleModel, AttrLoader(m_dataSource, handle),
                             m_loadAltitudes, IsTwoThreadsReady()), IsTwoThreadsReady());
  return graph;
}

IndexGraphLoaderImpl::GraphAttrs & IndexGraphLoaderImpl::CreateIndexGraph(
    NumMwmId numMwmId, GraphAttrs & graph)
{
  CHECK(graph.m_geometry, ());
  platform::CountryFile const & file = m_numMwmIds->GetFile(numMwmId);
  MwmSet::MwmHandle handle = m_dataSource.GetMwmHandleByCountryFile(file);
  if (!handle.IsAlive())
    MYTHROW(RoutingException, ("Can't get mwm handle for", file));

  graph.m_indexGraph = make_unique<IndexGraph>(graph.m_geometry, m_estimator, m_avoidRoutingOptions);
  graph.m_indexGraph->SetCurrentTimeGetter(m_currentTimeGetter);
  base::Timer timer;
  MwmValue const & mwmValue = *handle.GetValue();
  DeserializeIndexGraph(mwmValue, m_vehicleType, *graph.m_indexGraph);
  LOG(LINFO, (ROUTING_FILE_TAG, "section for", file.GetName(), "loaded in", timer.ElapsedSeconds(),
      "seconds"));
  return graph;
}

void IndexGraphLoaderImpl::Clear()
{
  base::OptionalLockGuard guard(m_graphsMtx);
  m_graphs.clear();
}

bool IndexGraphLoaderImpl::IsTwoThreadsReady() const { return m_graphsMtx.has_value(); }

bool ReadRoadAccessFromMwm(MwmValue const & mwmValue, VehicleType vehicleType,
                           RoadAccess & roadAccess)
{
  if (!mwmValue.m_cont.IsExist(ROAD_ACCESS_FILE_TAG))
    return false;

  try
  {
    auto const reader = mwmValue.m_cont.GetReader(ROAD_ACCESS_FILE_TAG);
    ReaderSource<FilesContainerR::TReader> src(reader);

    RoadAccessSerializer::Deserialize(src, vehicleType, roadAccess,
                                      mwmValue.m_file.GetPath(MapFileType::Map));
  }
  catch (Reader::OpenException const & e)
  {
    LOG(LERROR, ("Error while reading", ROAD_ACCESS_FILE_TAG, "section.", e.Msg()));
    return false;
  }
  return true;
}
}  // namespace

namespace routing
{
// static
unique_ptr<IndexGraphLoader> IndexGraphLoader::Create(
    VehicleType vehicleType, bool loadAltitudes, bool isTwoThreadsReady, shared_ptr<NumMwmIds> numMwmIds,
    shared_ptr<VehicleModelFactoryInterface> vehicleModelFactory,
    shared_ptr<EdgeEstimator> estimator, DataSource & dataSource,
    RoutingOptions routingOptions)
{
  return make_unique<IndexGraphLoaderImpl>(vehicleType, loadAltitudes, isTwoThreadsReady, numMwmIds,
                                           vehicleModelFactory, estimator, dataSource,
                                           routingOptions);
}

void DeserializeIndexGraph(MwmValue const & mwmValue, VehicleType vehicleType, IndexGraph & graph)
{
  FilesContainerR::TReader reader(mwmValue.m_cont.GetReader(ROUTING_FILE_TAG));
  ReaderSource<FilesContainerR::TReader> src(reader);
  IndexGraphSerializer::Deserialize(graph, src, GetVehicleMask(vehicleType));
  RestrictionLoader restrictionLoader(mwmValue, graph);
  if (restrictionLoader.HasRestrictions())
  {
    graph.SetRestrictions(restrictionLoader.StealRestrictions());
    graph.SetUTurnRestrictions(restrictionLoader.StealNoUTurnRestrictions());
  }

  RoadAccess roadAccess;
  if (ReadRoadAccessFromMwm(mwmValue, vehicleType, roadAccess))
    graph.SetRoadAccess(move(roadAccess));
}
}  // namespace routing
