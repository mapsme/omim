#pragma once

#include "traffic/traffic_cache.hpp"

#include "routing/index_router.hpp"
#include "routing/routing_callbacks.hpp"
#include "routing/segment.hpp"
#include "routing/vehicle_mask.hpp"
#include "routing_common/num_mwm_id.hpp"

#include "routing/base/followed_polyline.hpp"

#include "storage/country_info_getter.hpp"
#include "storage/country_info_reader_light.hpp"
#include "storage/country_parent_getter.hpp"

#include "platform/platform.hpp"

#include "geometry/latlon.hpp"

#include "base/thread_pool_computational.hpp"

#include <future>
#include <memory>
#include <vector>

#include <sys/types.h>

namespace routing
{
namespace routes_builder
{
class RoutesBuilder
{
public:
  explicit RoutesBuilder(size_t threadsNumber);

  static RoutesBuilder & GetSimpleRoutesBuilder();

  struct Params
  {
    Params(VehicleType type, ms::LatLon const & s, ms::LatLon const & f);

    VehicleType m_type;
    ms::LatLon m_start;
    ms::LatLon m_finish;
  };

  struct Result
  {
    static void Dump(Result const & result, std::string const & filePath);
    static Result Load(std::string const & filePath);

    ms::LatLon m_start;
    ms::LatLon m_finish;
    RouterResultCode m_code;
    double m_eta;
    double m_distance;
    std::vector<Segment> m_segments;
    FollowedPolyline m_followedPolyline;
  };

  Result ProcessTask(Params const & params);
  std::future<Result> ProcessTaskAsync(Params const & params);

private:

  class Processor
  {
  public:
    Processor(std::weak_ptr<storage::CountryParentGetter> cpg,
              std::weak_ptr<storage::CountryInfoGetter> cig,
              std::shared_ptr<NumMwmIds> numMwmIds,
              FrozenDataSource & dataSource);

    Result operator()(Params const & params, size_t taskNumber = 0);

  private:
    void InitRouter(VehicleType type);

    ms::LatLon m_start;
    ms::LatLon m_finish;

    std::shared_ptr<IndexRouter> m_router;
    std::shared_ptr<RouterDelegate> m_delegate = std::make_shared<RouterDelegate>();

    std::shared_ptr<NumMwmIds> m_numMwmIds;
    std::shared_ptr<traffic::TrafficCache> m_trafficCache = std::make_shared<traffic::TrafficCache>();
    FrozenDataSource & m_dataSource;
    std::weak_ptr<storage::CountryParentGetter> m_cpg;
    std::weak_ptr<storage::CountryInfoGetter> m_cig;
  };

  base::thread_pool::computational::ThreadPool m_threadPool;

  std::shared_ptr<storage::CountryParentGetter> m_cpg =
      std::make_shared<storage::CountryParentGetter>();

  std::shared_ptr<storage::CountryInfoGetter> m_cig =
      storage::CountryInfoReader::CreateCountryInfoReaderShared(GetPlatform());

  std::shared_ptr<NumMwmIds> m_numMwmIds = std::make_shared<NumMwmIds>();
  FrozenDataSource m_dataSource;

  size_t m_taskNumber = 0;
};
}  // namespace routes_builder
}  // namespace
