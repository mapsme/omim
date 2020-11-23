#include "platform/location.hpp"
#include "platform/settings.hpp"

#include "geometry/mercator.hpp"

namespace location_helpers
{

static inline BOOL isMyPositionPendingOrNoPosition()
{
  location::EMyPositionMode mode;
  if (!settings::Get(settings::kLocationStateMode, mode))
    return true;
  return mode == location::EMyPositionMode::PendingPosition ||
         mode == location::EMyPositionMode::NotFollowNoPosition;
}

static inline m2::PointD ToMercator(CLLocationCoordinate2D const & l)
{
  return mercator::FromLatLon(l.latitude, l.longitude);
}

}  // namespace MWMLocationHelpers
