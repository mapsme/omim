#pragma once

#include "map/user_mark.hpp"

#include "drape_frontend/user_marks_provider.hpp"

#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"
#include "geometry/any_rect2d.hpp"

#include "std/deque.hpp"
#include "std/bitset.hpp"

#include "std/noncopyable.hpp"
#include "std/unique_ptr.hpp"

class Framework;

enum class UserMarkType
{
  SEARCH_MARK,
  API_MARK,
  DEBUG_MARK,
  BOOKMARK_MARK,
  ROUTING_MARK,
  LOCAL_ADS_MARK
};

class UserMarksController
{
public:
  virtual size_t GetUserMarkCount() const = 0;
  virtual UserMarkType GetType() const = 0;
  virtual void SetIsDrawable(bool isDrawable) = 0;
  virtual void SetIsVisible(bool isVisible) = 0;

  virtual UserMark * CreateUserMark(m2::PointD const & ptOrg) = 0;
  virtual UserMark const * GetUserMark(size_t index) const = 0;
  virtual UserMark * GetUserMarkForEdit(size_t index) = 0;
  virtual void DeleteUserMark(size_t index) = 0;
  virtual void Clear(size_t skipCount = 0) = 0;
  virtual void Update() = 0;
  virtual void NotifyChanges() = 0;
};

class UserMarkContainer : public df::UserMarksProvider
                        , public UserMarksController
                        , private noncopyable
{
public:
  using TUserMarksList = deque<unique_ptr<UserMark>>;

  UserMarkContainer(double layerDepth, UserMarkType type, Framework & fm);
  virtual ~UserMarkContainer();

  // If not found mark on rect result is nullptr.
  // If mark is found in "d" return distance from rect center.
  // In multiple select choose mark with min(d).
  UserMark const * FindMarkInRect(m2::AnyRectD const & rect, double & d) const;

  static void InitStaticMarks(UserMarkContainer * container);
  static PoiMarkPoint * UserMarkForPoi();
  static MyPositionMarkPoint * UserMarkForMyPostion();

  // UserMarksProvider implementation.
  size_t GetUserPointCount() const override;
  df::UserPointMark const * GetUserPointMark(size_t index) const override;

  size_t GetUserLineCount() const override;
  df::UserLineMark const * GetUserLineMark(size_t index) const override;

  bool IsDirty() const override;

  // Discard isDirty flag, return id collection of removed marks since previous method call.
  void AcceptChanges(std::vector<uint32_t> & removedMarks) override;

  float GetPointDepth() const;

  bool IsVisible() const;
  bool IsDrawable() const override;
  size_t GetUserMarkCount() const override;
  UserMark const * GetUserMark(size_t index) const override;
  UserMarkType GetType() const override final;

  // UserMarksController implementation.
  UserMark * CreateUserMark(m2::PointD const & ptOrg) override;
  UserMark * GetUserMarkForEdit(size_t index) override;
  void DeleteUserMark(size_t index) override;
  void Clear(size_t skipCount = 0) override;
  void SetIsDrawable(bool isDrawable) override;
  void SetIsVisible(bool isVisible) override;
  void Update() override;
  void NotifyChanges() override;

protected:
  void SetDirty();

  virtual UserMark * AllocateUserMark(m2::PointD const & ptOrg) = 0;

  Framework & m_framework;

private:
  bitset<4> m_flags;
  double m_layerDepth;
  TUserMarksList m_userMarks;
  UserMarkType m_type;
  std::vector<uint32_t> m_removedMarks;
  bool m_isDirty = false;
};

class SearchUserMarkContainer : public UserMarkContainer
{
public:
  SearchUserMarkContainer(double layerDepth, Framework & framework);

protected:
  UserMark * AllocateUserMark(m2::PointD const & ptOrg) override;
};

class DebugUserMarkContainer : public UserMarkContainer
{
public:
  DebugUserMarkContainer(double layerDepth, Framework & framework);

protected:
  UserMark * AllocateUserMark(m2::PointD const & ptOrg) override;
};

class ApiUserMarkContainer : public UserMarkContainer
{
public:
  ApiUserMarkContainer(double layerDepth, Framework & framework);

protected:
  UserMark * AllocateUserMark(m2::PointD const & ptOrg) override;
};
