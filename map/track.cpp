#include "map/track.hpp"

#include "map/bookmark_helpers.hpp"
#include "map/user_mark_id_storage.hpp"

#include "geometry/mercator.hpp"

#include <utility>

Track::Track(kml::TrackData && data)
  : Base(data.m_id == kml::kInvalidTrackId ? UserMarkIdStorage::Instance().GetNextTrackId() : data.m_id)
  , m_data(std::move(data))
{
  m_data.m_id = GetId();
  CHECK_GREATER(m_data.m_pointsWithAltitudes.size(), 1, ());
  CacheLengthsAndLimitRect();
}

void Track::CacheLengthsAndLimitRect()
{
  m_cachedLimitRect.MakeEmpty();
  m_cachedLimitRect.Add(m_data.m_pointsWithAltitudes.front().GetPoint());
  m_cachedLengths.resize(m_data.m_pointsWithAltitudes.size());
  double length = 0.0;
  m_cachedLengths[0] = 0.0;
  for (size_t i = 1; i < m_data.m_pointsWithAltitudes.size(); ++i)
  {
    auto const & pt1 = m_data.m_pointsWithAltitudes[i - 1].GetPoint();
    auto const & pt2 = m_data.m_pointsWithAltitudes[i].GetPoint();
    auto const segmentLength = mercator::DistanceOnEarth(pt1, pt2);
    length += segmentLength;
    m_cachedLengths[i] = length;
    m_cachedLimitRect.Add(pt2);
  }
}

std::string Track::GetName() const
{
  return GetPreferredBookmarkStr(m_data.m_name);
}

m2::RectD const & Track::GetLimitRect() const
{
  return m_cachedLimitRect;
}

double Track::GetLengthMeters() const
{
  return m_cachedLengths.back();
}

double Track::GetLengthMeters(size_t pointIndex) const
{
  CHECK_LESS(pointIndex, m_cachedLengths.size(), (pointIndex, m_cachedLengths.size()));
  return m_cachedLengths[pointIndex];
}

df::DepthLayer Track::GetDepthLayer() const
{
  return df::DepthLayer::UserLineLayer;
}

size_t Track::GetLayerCount() const
{
  return m_data.m_layers.size();
}

dp::Color Track::GetColor(size_t layerIndex) const
{
  CHECK_LESS(layerIndex, m_data.m_layers.size(), ());
  return dp::Color(m_data.m_layers[layerIndex].m_color.m_rgba);
}

float Track::GetWidth(size_t layerIndex) const
{
  CHECK_LESS(layerIndex, m_data.m_layers.size(), ());
  return static_cast<float>(m_data.m_layers[layerIndex].m_lineWidth);
}

float Track::GetDepth(size_t layerIndex) const
{
  return layerIndex * 10;
}

std::vector<m2::PointD> Track::GetPoints() const
{
  std::vector<m2::PointD> result;
  result.reserve(m_data.m_pointsWithAltitudes.size());
  for (auto const & pt : m_data.m_pointsWithAltitudes)
    result.push_back(pt.GetPoint());

  return result;
}

std::vector<geometry::PointWithAltitude> const & Track::GetPointsWithAltitudes() const
{
  return m_data.m_pointsWithAltitudes;
}

void Track::Attach(kml::MarkGroupId groupId)
{
  ASSERT_EQUAL(m_groupID, kml::kInvalidMarkGroupId, ());
  m_groupID = groupId;
}

void Track::Detach()
{
  m_groupID = kml::kInvalidMarkGroupId;
}

void Track::SetSelectionMarkId(kml::MarkId markId)
{
  m_selectionMarkId = markId;
}

bool Track::GetPoint(double distanceInMeters, m2::PointD & pt) const
{
  CHECK_GREATER_OR_EQUAL(distanceInMeters, 0.0, (distanceInMeters));

  double const kEpsMeters = 1e-2;
  if (base::AlmostEqualAbs(distanceInMeters, m_cachedLengths.front(), kEpsMeters))
  {
    pt = m_data.m_pointsWithAltitudes.front().GetPoint();
    return true;
  }

  if (base::AlmostEqualAbs(distanceInMeters, m_cachedLengths.back(), kEpsMeters))
  {
    pt = m_data.m_pointsWithAltitudes.back().GetPoint();
    return true;
  }

  auto const it = std::lower_bound(m_cachedLengths.begin(), m_cachedLengths.end(), distanceInMeters);
  if (it == m_cachedLengths.end())
    return false;

  auto const pointIndex = std::distance(m_cachedLengths.begin(), it);
  auto const length = *it;

  auto const segmentLength = length - m_cachedLengths[pointIndex - 1];
  auto const k = (segmentLength - (length - distanceInMeters)) / segmentLength;

  auto const & pt1 = m_data.m_pointsWithAltitudes[pointIndex - 1].GetPoint();
  auto const & pt2 = m_data.m_pointsWithAltitudes[pointIndex].GetPoint();
  pt = pt1 + (pt2 - pt1) * k;

  return true;
}
