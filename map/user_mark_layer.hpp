#pragma once

#include "map/user_mark.hpp"

#include "drape_frontend/drape_engine_safe_ptr.hpp"

#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"
#include "geometry/any_rect2d.hpp"

#include <base/macros.hpp>

#include <functional>
#include <memory>
#include <set>

class UserMarkLayer
{
public:
  UserMarkLayer(UserMark::Type type);
  virtual ~UserMarkLayer();

  bool IsDirty() const { return m_isDirty; }
  void ResetChanges();

  bool IsVisible() const;
  bool IsVisibilityChanged() const;
  UserMark::Type GetType() const;

  df::MarkIDSet const & GetUserMarks() const { return m_userMarks; }
  df::LineIDSet const & GetUserLines() const { return m_tracks; }

  void AttachUserMark(df::MarkID markId);
  void DetachUserMark(df::MarkID markId);

  void AttachTrack(df::LineID trackId);
  void DetachTrack(df::LineID trackId);

  void Clear();
  bool IsEmpty() const;

  void SetIsVisible(bool isVisible);

protected:
  void SetDirty() { m_isDirty = true; }

  UserMark::Type m_type;

  df::MarkIDSet m_userMarks;
  df::LineIDSet m_tracks;

  bool m_isDirty = true;
  bool m_isVisible = true;
  bool m_wasVisible = false;

  DISALLOW_COPY_AND_MOVE(UserMarkLayer);
};

