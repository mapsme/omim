#include "routing/index_graph_loader.hpp"

#include "routing/deserialize_speed_cameras.hpp"
#include "routing/index_graph_serialization.hpp"
#include "routing/restriction_loader.hpp"
#include "routing/road_access_serialization.hpp"
#include "routing/route.hpp"
#include "routing/routing_exceptions.hpp"

#include "indexer/data_source.hpp"

#include "coding/file_container.hpp"

#include "base/assert.hpp"
#include "base/timer.hpp"

#include <map>
#include <unordered_map>

namespace
{
using namespace routing;
using namespace std;

class IndexGraphLoaderImpl final : public IndexGraphLoader
{
public:
  IndexGraphLoaderImpl(VehicleType vehicleType, bool loadAltitudes, shared_ptr<NumMwmIds> numMwmIds,
                       shared_ptr<VehicleModelFactoryInterface> vehicleModelFactory,
                       shared_ptr<EdgeEstimator> estimator, DataSource & dataSource);

  // IndexGraphLoader overrides:
  Geometry & GetGeometry(NumMwmId numMwmId) override;
  IndexGraph & GetIndexGraph(NumMwmId numMwmId) override;
  vector<RouteSegment::SpeedCamera> & GetSpeedCameraInfo(Segment const & segment) override;
  void Clear() override;

private:
  struct GeometryIndexGraph
  {
    shared_ptr<Geometry> m_geometry;
    unique_ptr<IndexGraph> m_indexGraph;
  };

  GeometryIndexGraph & CreateGeometry(NumMwmId numMwmId);
  GeometryIndexGraph & CreateIndexGraph(NumMwmId numMwmId, GeometryIndexGraph & graph);

  VehicleType m_vehicleType;
  bool m_loadAltitudes;
  DataSource & m_dataSource;
  shared_ptr<NumMwmIds> m_numMwmIds;
  shared_ptr<VehicleModelFactoryInterface> m_vehicleModelFactory;
  shared_ptr<EdgeEstimator> m_estimator;
  unordered_map<NumMwmId, GeometryIndexGraph> m_graphs;

  unordered_map<NumMwmId, map<SegmentCoord, vector<RouteSegment::SpeedCamera>>> m_cachedCameras;
};

IndexGraphLoaderImpl::IndexGraphLoaderImpl(
    VehicleType vehicleType, bool loadAltitudes, shared_ptr<NumMwmIds> numMwmIds,
    shared_ptr<VehicleModelFactoryInterface> vehicleModelFactory,
    shared_ptr<EdgeEstimator> estimator, DataSource & dataSource)
  : m_vehicleType(vehicleType)
  , m_loadAltitudes(loadAltitudes)
  , m_dataSource(dataSource)
  , m_numMwmIds(numMwmIds)
  , m_vehicleModelFactory(vehicleModelFactory)
  , m_estimator(estimator)
{
  CHECK(m_numMwmIds, ());
  CHECK(m_vehicleModelFactory, ());
  CHECK(m_estimator, ());
}

Geometry & IndexGraphLoaderImpl::GetGeometry(NumMwmId numMwmId)
{
  auto it = m_graphs.find(numMwmId);
  if (it != m_graphs.end())
    return *it->second.m_geometry;

  return *CreateGeometry(numMwmId).m_geometry;
}

IndexGraph & IndexGraphLoaderImpl::GetIndexGraph(NumMwmId numMwmId)
{
  auto it = m_graphs.find(numMwmId);
  if (it != m_graphs.end())
  {
    return it->second.m_indexGraph ? *it->second.m_indexGraph
                                   : *CreateIndexGraph(numMwmId, it->second).m_indexGraph;
  }

  return *CreateIndexGraph(numMwmId, CreateGeometry(numMwmId)).m_indexGraph;
}

vector<RouteSegment::SpeedCamera> & IndexGraphLoaderImpl::GetSpeedCameraInfo(Segment const & segment)
{
  auto const numMwmId = segment.GetMwmId();

  auto it = m_cachedCameras.find(numMwmId);
  if (it == m_cachedCameras.end())
  {
    m_cachedCameras.emplace(numMwmId,
                            map<SegmentCoord, vector<RouteSegment::SpeedCamera>>{});
    auto & mapReference = m_cachedCameras.find(numMwmId)->second;

    auto const & file = m_numMwmIds->GetFile(numMwmId);
    auto handle = m_dataSource.GetMwmHandleByCountryFile(file);
    if (!handle.IsAlive())
      MYTHROW(RoutingException, ("Can't get mwm handle for", file));

    try
    {
      MwmValue const & mwmValue = *handle.GetValue<MwmValue>();
      FilesContainerR::TReader reader(mwmValue.m_cont.GetReader(CAMERAS_INFO_FILE_TAG));
      ReaderSource<FilesContainerR::TReader> src(reader);
      DeserializeSpeedCamsFromMwm(src, mapReference);
    }
    catch (Reader::OpenException & ex)
    {
      LOG(LINFO, ("No section about speed cameras"));
      return kEmptyVectorOfSpeedCameras;
    }

    it = m_cachedCameras.find(numMwmId);
  }

  auto result = it->second.find({segment.GetFeatureId(), segment.GetSegmentIdx()});
  if (result == it->second.end())
    return kEmptyVectorOfSpeedCameras;

  return result->second;
}

IndexGraphLoaderImpl::GeometryIndexGraph & IndexGraphLoaderImpl::CreateGeometry(NumMwmId numMwmId)
{
  platform::CountryFile const & file = m_numMwmIds->GetFile(numMwmId);
  MwmSet::MwmHandle handle = m_dataSource.GetMwmHandleByCountryFile(file);
  if (!handle.IsAlive())
    MYTHROW(RoutingException, ("Can't get mwm handle for", file));

  shared_ptr<VehicleModelInterface> vehicleModel =
      m_vehicleModelFactory->GetVehicleModelForCountry(file.GetName());

  auto & graph = m_graphs[numMwmId];
  graph.m_geometry =
      make_shared<Geometry>(GeometryLoader::Create(m_dataSource, handle, vehicleModel, m_loadAltitudes));
  return graph;
}

IndexGraphLoaderImpl::GeometryIndexGraph & IndexGraphLoaderImpl::CreateIndexGraph(
    NumMwmId numMwmId, GeometryIndexGraph & graph)
{
  CHECK(graph.m_geometry, ());
  platform::CountryFile const & file = m_numMwmIds->GetFile(numMwmId);
  MwmSet::MwmHandle handle = m_dataSource.GetMwmHandleByCountryFile(file);
  if (!handle.IsAlive())
    MYTHROW(RoutingException, ("Can't get mwm handle for", file));

  graph.m_indexGraph = make_unique<IndexGraph>(graph.m_geometry, m_estimator);
  my::Timer timer;
  MwmValue const & mwmValue = *handle.GetValue<MwmValue>();
  DeserializeIndexGraph(mwmValue, m_vehicleType, *graph.m_indexGraph);
  LOG(LINFO, (ROUTING_FILE_TAG, "section for", file.GetName(), "loaded in", timer.ElapsedSeconds(),
      "seconds"));
  return graph;
}

void IndexGraphLoaderImpl::Clear() { m_graphs.clear(); }

bool ReadRoadAccessFromMwm(MwmValue const & mwmValue, VehicleType vehicleType,
                           RoadAccess & roadAccess)
{
  if (!mwmValue.m_cont.IsExist(ROAD_ACCESS_FILE_TAG))
    return false;

  try
  {
    auto const reader = mwmValue.m_cont.GetReader(ROAD_ACCESS_FILE_TAG);
    ReaderSource<FilesContainerR::TReader> src(reader);

    RoadAccessSerializer::Deserialize(src, vehicleType, roadAccess);
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
    VehicleType vehicleType, bool loadAltitudes, shared_ptr<NumMwmIds> numMwmIds,
    shared_ptr<VehicleModelFactoryInterface> vehicleModelFactory,
    shared_ptr<EdgeEstimator> estimator, DataSource & dataSource)
{
  return make_unique<IndexGraphLoaderImpl>(vehicleType, loadAltitudes, numMwmIds, vehicleModelFactory,
                                           estimator, dataSource);
}

void DeserializeIndexGraph(MwmValue const & mwmValue, VehicleType vehicleType, IndexGraph & graph)
{
  FilesContainerR::TReader reader(mwmValue.m_cont.GetReader(ROUTING_FILE_TAG));
  ReaderSource<FilesContainerR::TReader> src(reader);
  IndexGraphSerializer::Deserialize(graph, src, GetVehicleMask(vehicleType));
  RestrictionLoader restrictionLoader(mwmValue, graph);
  if (restrictionLoader.HasRestrictions())
    graph.SetRestrictions(restrictionLoader.StealRestrictions());

  RoadAccess roadAccess;
  if (ReadRoadAccessFromMwm(mwmValue, vehicleType, roadAccess))
    graph.SetRoadAccess(move(roadAccess));
}
}  // namespace routing
