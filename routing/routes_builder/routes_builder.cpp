#include "routing/routes_builder/routes_builder.hpp"

#include "routing/vehicle_mask.hpp"

#include "storage/routing_helpers.hpp"

#include "indexer/classificator_loader.hpp"
#include "indexer/mwm_set.hpp"

#include "platform/local_country_file_utils.cpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

#include <limits>
#include <utility>

namespace routing
{
namespace routes_builder
{
// static
RoutesBuilder & RoutesBuilder::GetSimpleRoutesBuilder()
{
  static RoutesBuilder routesBuilder(1 /* threadsNumber */);
  return routesBuilder;
}

RoutesBuilder::Params::Params(VehicleType type, ms::LatLon const & s, ms::LatLon const & f)
  : m_type(type), m_start(s), m_finish(f) {}

RoutesBuilder::RoutesBuilder(size_t threadsNumber)
  : m_threadPool(threadsNumber)
{
  LOG(LINFO, ("Threads number:", threadsNumber));
  CHECK(m_cig, ());
  CHECK(m_cpg, ());

  classificator::Load();
  vector<platform::LocalCountryFile> localFiles;
  platform::FindAllLocalMapsAndCleanup(std::numeric_limits<int64_t>::max(), localFiles);

  for (auto const & localFile : localFiles)
  {
    UNUSED_VALUE(m_dataSource.RegisterMap(localFile));
    auto const & countryFile = localFile.GetCountryFile();
    auto const mwmId = m_dataSource.GetMwmIdByCountryFile(countryFile);
    CHECK(mwmId.IsAlive(), ());
    // We have to exclude minsk-pass because we can't register mwm which is not from
    // countries.txt.
    if (mwmId.GetInfo()->GetType() == MwmInfo::COUNTRY && countryFile.GetName() != "minsk-pass")
      m_numMwmIds->RegisterFile(countryFile);
  }
}

// static
void RoutesBuilder::Result::Dump(Result const & result, std::string const & filePath)
{
  std::ofstream output(filePath);
  CHECK(output.good(), ("Error during opening:", filePath));

  output << std::setprecision(20);

  output << static_cast<int>(result.m_code) << "\n"
         << result.m_start.m_lat << " " << result.m_start.m_lon << "\n"
         << result.m_finish.m_lat << " " << result.m_finish.m_lon << "\n"
         << result.m_eta << " " << result.m_distance << "\n";

  output << result.m_segments.size() << "\n";
  for (auto const & segment : result.m_segments)
  {
    output << segment.GetMwmId() << " "
           << segment.GetFeatureId() << " "
           << segment.GetSegmentIdx() << " "
           << segment.IsForward() << "\n";
  }
}

// static
RoutesBuilder::Result RoutesBuilder::Result::Load(std::string const & filePath)
{
  Result result;
  std::ifstream input(filePath);
  CHECK(input.good(), ("Error during opening:", filePath));

  int code;
  input >> code;
  result.m_code = static_cast<RouterResultCode>(code);

  input >> result.m_start.m_lat >> result.m_start.m_lon
        >> result.m_finish.m_lat >> result.m_finish.m_lon
        >> result.m_eta >> result.m_distance;

  size_t n;
  input >> n;
  result.m_segments.resize(n);
  for (size_t i = 0; i < n; ++i)
  {
    NumMwmId numMwmId;
    uint32_t featureId;
    uint32_t segmentId;
    bool forward;
    input >> numMwmId >> featureId >> segmentId >> forward;

    result.m_segments[i] = Segment(numMwmId, featureId, segmentId, forward);
  }

  std::vector<m2::PointD> routeGeometry(n + 1);
  for (size_t i = 0; i < routeGeometry.size(); ++i)
    input >> routeGeometry[i].x >> routeGeometry[i].y;

  result.m_followedPolyline = FollowedPolyline(routeGeometry.begin(), routeGeometry.end());

  return result;
}

RoutesBuilder::Processor::Processor(std::weak_ptr<storage::CountryParentGetter> cpg,
                                    std::weak_ptr<storage::CountryInfoGetter> cig,
                                    std::shared_ptr<NumMwmIds> numMwmIds,
                                    FrozenDataSource & dataSource)
  : m_numMwmIds(std::move(numMwmIds))
  , m_dataSource(dataSource)
  , m_cpg(std::move(cpg))
  , m_cig(std::move(cig)) {}

void RoutesBuilder::Processor::InitRouter(VehicleType type)
{
  if (m_router && m_router->GetVehicleType() == type)
    return;

  auto const & cig = m_cig;
  auto const countryFileGetter = [cig](m2::PointD const & pt) {
    auto const cigSharedPtr = cig.lock();
    return cigSharedPtr->GetRegionCountryId(pt);
  };

  auto const getMwmRectByName = [cig](string const & countryId) {
    auto const cigSharedPtr = cig.lock();
    return cigSharedPtr->GetLimitRectForLeaf(countryId);
  };

  m_router = std::make_shared<IndexRouter>(type,
                                           false /* load altitudes */,
                                           *m_cpg.lock(),
                                           countryFileGetter,
                                           getMwmRectByName,
                                           m_numMwmIds,
                                           MakeNumMwmTree(*m_numMwmIds, *m_cig.lock()),
                                           *m_trafficCache,
                                           m_dataSource);
}

RoutesBuilder::Result
RoutesBuilder::Processor::operator()(Params const & params, size_t taskNumber /* = 0 */)
{
  InitRouter(params.m_type);

  if (taskNumber % 10 == 1)
    LOG(LINFO, ("Get task:", taskNumber));

  std::vector<m2::PointD> waypoints = {
      MercatorBounds::FromLatLon(params.m_start),
      MercatorBounds::FromLatLon(params.m_finish)
  };

  RouterResultCode resultCode;
  routing::Route route("" /* router */, 0 /* routeId */);

  resultCode =
      m_router->CalculateRoute(Checkpoints(std::move(waypoints)),
                               m2::PointD::Zero(),
                               false /* adjustToPrevRoute */,
                               *m_delegate,
                               route);

  Result result;
  result.m_start = params.m_start;
  result.m_finish = params.m_finish;
  result.m_code = resultCode;
  result.m_distance = route.GetTotalDistanceMeters();
  result.m_eta = route.GetTotalTimeSec();

  for (auto const & routeSegment : route.GetRouteSegments())
    result.m_segments.emplace_back(routeSegment.GetSegment());

  result.m_followedPolyline = route.GetFollowedPolyline();

  return result;
}

RoutesBuilder::Result RoutesBuilder::ProcessTask(Params const & params)
{
  Processor processor(m_cpg, m_cig, m_numMwmIds, m_dataSource);
  return processor(params);
}

std::future<RoutesBuilder::Result> RoutesBuilder::ProcessTaskAsync(Params const & params)
{
  ++m_taskNumber;
  Processor processor(m_cpg, m_cig, m_numMwmIds, m_dataSource);
  return m_threadPool.Submit(processor, params, m_taskNumber);
}
}  // namespace routes_builder
}  // namespace