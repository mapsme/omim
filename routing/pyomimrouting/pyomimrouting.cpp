#include "geometry/mercator.hpp"

#include "platform/local_country_file.hpp"
#include "platform/local_country_file_utils.hpp"
#include "platform/platform.hpp"

#include "routing/routing_integration_tests/routing_test_tools.hpp"

#include "std/string.hpp"

#include <boost/python.hpp>

using namespace integration;

// Dummy version of routing_test_tools.cpp::GetAllLocalFiles
void GetLocalFiles(const std::string & dataPath,
                   vector<LocalCountryFile> & localFiles)
{
  Platform & pl = GetPlatform();
  pl.SetWritableDirForTests(dataPath);
  pl.SetResourceDir(dataPath);

  platform::migrate::SetMigrationFlag();
  platform::FindAllLocalMapsAndCleanup(numeric_limits<int64_t>::max() /* latestVersion */,
                                       localFiles);
  for (auto & file : localFiles)
    file.SyncWithDisk();
}

// Dummy version of routing_test_tools.cpp::CreateAllMapsComponents
shared_ptr<VehicleRouterComponents> CreateLocalMapsComponents(
    const std::string & dataPath, VehicleType vehicleType)
{
  vector<LocalCountryFile> localFiles;
  GetLocalFiles(dataPath, localFiles);
  ASSERT(!localFiles.empty(), ());
  return make_shared<VehicleRouterComponents>(localFiles, vehicleType);
}

struct PyRouter
{
  PyRouter(std::string path) : dataPath(path) {}

  std::string dataPath;

  boost::python::tuple calculate_route(VehicleType vehicleType,
                                       float startX,
                                       float startY,
                                       float finishX,
                                       float finishY) {
    static auto const components = CreateLocalMapsComponents(this->dataPath,
                                                             vehicleType);

    ASSERT(components, ());

    auto resultRoute = CalculateRoute(
        *components,
        MercatorBounds::FromLatLon(startX, startY), {0., 0.},
        MercatorBounds::FromLatLon(finishX, finishY));

    return boost::python::make_tuple(resultRoute.second,
                                     resultRoute.first->GetTotalDistanceMeters());
  }
};


BOOST_PYTHON_MODULE(pyomimrouting)
{
    using namespace boost::python;

    enum_<VehicleType>("route_types")
        .value("pedestrian", VehicleType::Pedestrian)
        .value("bicycle", VehicleType::Bicycle)
        .value("car", VehicleType::Car)
        .value("transit", VehicleType::Transit)
        .value("count", VehicleType::Count)
    ;

    // Naming convention like exceptions
    enum_<IRouter::ResultCode>("codes")
        .value("NoError", IRouter::NoError)
        .value("Cancelled", IRouter::Cancelled)
        .value("NoCurrentPosition", IRouter::NoCurrentPosition)
        .value("InconsistentMWMandRoute", IRouter::InconsistentMWMandRoute)
        .value("RouteFileNotExist", IRouter::RouteFileNotExist)
        .value("StartPointNotFound", IRouter::StartPointNotFound)
        .value("EndPointNotFound", IRouter::EndPointNotFound)
        .value("PointsInDifferentMWM", IRouter::PointsInDifferentMWM)
        .value("RouteNotFound", IRouter::RouteNotFound)
        .value("NeedMoreMaps", IRouter::NeedMoreMaps)
        .value("InternalError", IRouter::InternalError)
        .value("FileTooOld", IRouter::FileTooOld)
        .value("IntermediatePointNotFound", IRouter::IntermediatePointNotFound)
        .value("TransitRouteNotFoundNoNetwork",
               IRouter::TransitRouteNotFoundNoNetwork)
        .value("TransitRouteNotFoundTooLongPedestrian",
               IRouter::TransitRouteNotFoundTooLongPedestrian)
        .value("RouteNotFoundRedressRouteError",
               IRouter::TransitRouteNotFoundTooLongPedestrian)
    ;

    class_<PyRouter>("PyRouter", init<std::string>())
        .def("calculate_route", &PyRouter::calculate_route)
    ;
}
