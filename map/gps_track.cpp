#include "map/gps_track.hpp"

#include "indexer/mercator.hpp"

#include "coding/file_name_utils.hpp"

#include "platform/platform.hpp"

namespace
{

char const kDefaultGpsFileName[] = "gpstrack.bin";

size_t const kGpsFileMaxItemCount = 100000;
hours const kDefaultDuration = hours(24);

} // namespace

GpsTrack::GpsTrack(string const & filePath)
  : m_filePath(filePath)
  , m_duration(kDefaultDuration)
{
}

void GpsTrack::AddPoint(location::GpsTrackInfo const & info)
{
  lock_guard<mutex> lg(m_guard);

  m2::PointD const pt = MercatorBounds::FromLatLon(ms::LatLon(info.m_latitude, info.m_longitude));

  InitFile();

  size_t poppedId;
  size_t addedId = m_file->Append(info.m_timestamp, pt, info.m_speed, poppedId);
  if (addedId == GpsTrackFile::kInvalidId)
    return;

  if (m_container)
    m_container->AddPoint(pt, info.m_timestamp, info.m_speed);
}

void GpsTrack::AddPoints(vector<location::GpsTrackInfo> const & infos)
{
  if (infos.empty())
    return;

  lock_guard<mutex> lg(m_guard);

  InitFile();

  for (auto const & info : infos)
  {
    m2::PointD const pt = MercatorBounds::FromLatLon(ms::LatLon(info.m_latitude, info.m_longitude));

    size_t poppedId;
    size_t addedId = m_file->Append(info.m_timestamp, pt, info.m_speed, poppedId);
    if (addedId == GpsTrackFile::kInvalidId)
      return;

    if (m_container)
      m_container->AddPoint(pt, info.m_timestamp, info.m_speed);
  }
}

void GpsTrack::Clear()
{
  lock_guard<mutex> lg(m_guard);

  if (m_container)
    m_container->Clear();

  InitFile();

  m_file->Clear();
}

void GpsTrack::SetDuration(hours duration)
{
  lock_guard<mutex> lg(m_guard);

  if (m_container)
    m_container->SetDuration(duration);

  m_duration = duration;
}

hours GpsTrack::GetDuration() const
{
  lock_guard<mutex> lg(m_guard);

  return m_duration;
}

void GpsTrack::SetCallback(TGpsTrackDiffCallback callback)
{
  lock_guard<mutex> lg(m_guard);

  if (callback)
    InitContainer();

  m_container->SetCallback(callback, true /* sendAll */);
}

void GpsTrack::InitFile()
{
  // Must be called under m_guard lock

  if (m_file)
    return;

  m_file = make_unique<GpsTrackFile>(m_filePath, kGpsFileMaxItemCount);
}

void GpsTrack::InitContainer()
{
  // Must be called under m_guard lock

  if (m_container)
    return;

  InitFile();

  m_container = make_unique<GpsTrackContainer>();
  m_container->SetDuration(m_duration);

  try
  {
    m_file->ForEach([this](double timestamp, m2::PointD const & pt, double speedMPS, size_t)->bool
    {
      m_container->AddPoint(pt, speedMPS, timestamp);
      return true;
    });
  }
  catch (RootException &)
  {
    // File has been corrupted, therefore drop any data
    m_container->Clear();
    m_file->Clear();
  }
}

GpsTrack & GetDefaultGpsTrack()
{
  static GpsTrack instance(my::JoinFoldersToPath(GetPlatform().WritableDir(), kDefaultGpsFileName));
  return instance;
}
