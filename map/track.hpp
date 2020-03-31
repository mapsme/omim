#pragma once

#include "kml/types.hpp"

#include "drape_frontend/user_marks_provider.hpp"

#include <string>

class Track : public df::UserLineMark
{
  using Base = df::UserLineMark;
public:
  explicit Track(kml::TrackData && data);

  kml::MarkGroupId GetGroupId() const override { return m_groupID; }

  bool IsDirty() const override { return m_isDirty; }
  void ResetChanges() const override { m_isDirty = false; }

  kml::TrackData const & GetData() const { return m_data; }

  std::string GetName() const;
  m2::RectD const & GetLimitRect() const;
  double GetLengthMeters() const;
  double GetLengthMeters(size_t pointIndex) const;

  int GetMinZoom() const override { return 1; }
  df::DepthLayer GetDepthLayer() const override;
  size_t GetLayerCount() const override;
  dp::Color GetColor(size_t layerIndex) const override;
  float GetWidth(size_t layerIndex) const override;
  float GetDepth(size_t layerIndex) const override;
  std::vector<m2::PointD> GetPoints() const override;
  std::vector<geometry::PointWithAltitude> const & GetPointsWithAltitudes() const;

  void Attach(kml::MarkGroupId groupId);
  void Detach();

  kml::MarkId GetSelectionMarkId() const { return m_selectionMarkId; }
  void SetSelectionMarkId(kml::MarkId markId);
  bool GetPoint(double distanceInMeters, m2::PointD & pt) const;

private:
  void CacheLengthsAndLimitRect();

  kml::TrackData m_data;
  kml::MarkGroupId m_groupID = kml::kInvalidMarkGroupId;
  kml::MarkId m_selectionMarkId = kml::kInvalidMarkId;
  std::vector<double> m_cachedLengths;
  m2::RectD m_cachedLimitRect;
  mutable bool m_isDirty = true;
};
