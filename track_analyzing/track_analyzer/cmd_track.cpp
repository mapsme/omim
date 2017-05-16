#include "track_analyzing/track.hpp"
#include "track_analyzing/utils.hpp"

#include "map/routing_helpers.hpp"

#include "routing/geometry.hpp"

#include "routing_common/car_model.hpp"
#include "routing_common/vehicle_model.hpp"

#include "indexer/feature.hpp"
#include "indexer/features_vector.hpp"

#include "storage/storage.hpp"

#include "coding/file_name_utils.hpp"

#include "geometry/mercator.hpp"

using namespace routing;
using namespace std;

namespace tracking
{
void CmdTrack(string const & trackFile, string const & mwmName, string const & user,
              size_t trackIdx)
{
  storage::Storage storage;
  auto const numMwmIds = CreateNumMwmIds(storage);
  MwmToMatchedTracks mwmToMatchedTracks;
  ReadTracks(numMwmIds, trackFile, mwmToMatchedTracks);

  MatchedTrack const & track =
      GetMatchedTrack(mwmToMatchedTracks, *numMwmIds, mwmName, user, trackIdx);

  string const mwmFile =
      my::JoinPath(GetPlatform().WritableDir(), to_string(storage.GetCurrentDataVersion()), mwmName + DATA_FILE_EXTENSION);
  shared_ptr<IVehicleModel> vehicleModel = CarModelFactory().GetVehicleModelForCountry(mwmName);
  FeaturesVectorTest featuresVector(FilesContainerR(make_unique<FileReader>(mwmFile)));
  Geometry geometry(GeometryLoader::CreateFromFile(mwmFile, vehicleModel));

  uint64_t const duration =
      track.back().GetDataPoint().m_timestamp - track.front().GetDataPoint().m_timestamp;
  double const length = CalcTrackLength(track, geometry);
  double const averageSpeed = CalcSpeedKMpH(length, duration);
  LOG(LINFO, ("Mwm:", mwmName, ", user:", user, ", points:", track.size(), "duration:", duration,
              "length:", length, ", speed:", averageSpeed, "km/h"));

  for (size_t i = 0; i < track.size(); ++i)
  {
    MatchedTrackPoint const & point = track[i];
    FeatureType feature;
    featuresVector.GetVector().GetByIndex(point.GetSegment().GetFeatureId(), feature);

    double speed = 0.0;
    uint64_t elapsed = 0;
    double distance = 0.0;
    if (i > 0)
    {
      MatchedTrackPoint const & prevPoint = track[i - 1];
      elapsed = point.GetDataPoint().m_timestamp - prevPoint.GetDataPoint().m_timestamp;
      distance = MercatorBounds::DistanceOnEarth(
          MercatorBounds::FromLatLon(prevPoint.GetDataPoint().m_latLon),
          MercatorBounds::FromLatLon(point.GetDataPoint().m_latLon));
    }

    if (elapsed != 0)
      speed = CalcSpeedKMpH(distance, elapsed);

    LOG(LINFO, (my::SecondsSinceEpochToString(point.GetDataPoint().m_timestamp),
                point.GetDataPoint().m_latLon, point.GetSegment(), ", traffic:",
                point.GetDataPoint().m_traffic, ", distance:", distance, ", elapsed:", elapsed,
                ", speed:", speed));
  }
}
}  // namespace tracking
