#pragma once

#include "routing/routing_callbacks.hpp"
#include "routing/checkpoints.hpp"
#include "routing/route.hpp"
#include "routing/router_delegate.hpp"

#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"

#include "base/cancellable.hpp"

#include <functional>
#include <string>

namespace routing
{

using TCountryFileFn = std::function<std::string(m2::PointD const &)>;
using CourntryRectFn = std::function<m2::RectD(std::string const & countryId)>;
using CountryParentNameGetterFn = std::function<std::string(std::string const &)>;

class Route;

/// Routing engine type.
enum class RouterType
{
  // @TODO It's necessary to rename Vehicle value to Car.
  Vehicle = 0,  /// For Car routing.
  Pedestrian,   /// For A star pedestrian routing.
  Bicycle,      /// For A star bicycle routing.
  Taxi,         /// For taxi route calculation Vehicle routing is used.
  Transit,      /// For A star pedestrian + transit routing.
  Count         /// Number of router types.
};

std::string ToString(RouterType type);
RouterType FromString(std::string const & str);
std::string DebugPrint(RouterType type);

class IRouter
{
public:
  virtual ~IRouter() {}

  /// Return unique name of a router implementation.
  virtual std::string GetName() const = 0;

  /// Clear all temporary buffers.
  virtual void ClearState() {}

  /// Override this function with routing implementation.
  /// It will be called in separate thread and only one function will processed in same time.
  /// @warning please support Cancellable interface calls. You must stop processing when it is true.
  ///
  /// @param startPoint point to start routing
  /// @param startDirection start direction for routers with high cost of the turnarounds
  /// @param finalPoint target point for route
  /// @param adjust adjust route to the previous one if possible
  /// @param delegate callback functions and cancellation flag
  /// @param route result route
  /// @return ResultCode error code or NoError if route was initialised
  /// @see Cancellable
  virtual RouterResultCode CalculateRoute(Checkpoints const & checkpoints,
                                          m2::PointD const & startDirection, bool adjust,
                                          RouterDelegate const & delegate, Route & route) = 0;
};

}  // namespace routing
