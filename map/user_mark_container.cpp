#include "map/user_mark_container.hpp"
#include "map/search_mark.hpp"

#include "drape_frontend/drape_engine.hpp"
#include "drape_frontend/tile_key.hpp"

#include "base/macros.hpp"
#include "base/scope_guard.hpp"
#include "base/stl_add.hpp"

#include <algorithm>
#include <utility>

namespace
{
class FindMarkFunctor
{
public:
  FindMarkFunctor(UserMark ** mark, double & minD, m2::AnyRectD const & rect)
    : m_mark(mark)
    , m_minD(minD)
    , m_rect(rect)
  {
    m_globalCenter = rect.GlobalCenter();
  }

  void operator()(UserMark * mark)
  {
    m2::PointD const & org = mark->GetPivot();
    if (m_rect.IsPointInside(org))
    {
      double minDCandidate = m_globalCenter.SquareLength(org);
      if (minDCandidate < m_minD)
      {
        *m_mark = mark;
        m_minD = minDCandidate;
      }
    }
  }

  UserMark ** m_mark;
  double & m_minD;
  m2::AnyRectD const & m_rect;
  m2::PointD m_globalCenter;
};

size_t const VisibleFlag = 0;
size_t const DrawableFlag = 1;

df::MarkGroupID GenerateMarkGroupId(UserMarkContainer const * cont)
{
  return reinterpret_cast<df::MarkGroupID>(cont);
}
}  // namespace

UserMarkContainer::UserMarkContainer(double layerDepth, UserMark::Type type)
  : m_layerDepth(layerDepth)
  , m_type(type)
{
  m_flags.set();
}

UserMarkContainer::~UserMarkContainer()
{
  Clear();
  NotifyChanges();
}

void UserMarkContainer::SetDrapeEngine(ref_ptr<df::DrapeEngine> engine)
{
  m_drapeEngine.Set(engine);
}

UserMark const * UserMarkContainer::FindMarkInRect(m2::AnyRectD const & rect, double & d) const
{
  UserMark * mark = nullptr;
  if (IsVisible())
  {
    FindMarkFunctor f(&mark, d, rect);
    for (size_t i = 0; i < m_userMarks.size(); ++i)
    {
      if (m_userMarks[i]->IsAvailableForSearch() && rect.IsPointInside(m_userMarks[i]->GetPivot()))
         f(m_userMarks[i].get());
    }
  }
  return mark;
}

void UserMarkContainer::NotifyChanges()
{
  if (!IsDirty())
    return;

  df::DrapeEngineLockGuard lock(m_drapeEngine);
  if (!lock)
    return;

  auto engine = lock.Get();

  df::MarkGroupID const groupId = GenerateMarkGroupId(this);
  engine->ChangeVisibilityUserMarksGroup(groupId, IsVisible() && IsDrawable());

  if (GetUserPointCount() == 0 && GetUserLineCount() == 0)
  {
    engine->UpdateUserMarksGroup(groupId, this);
    engine->ClearUserMarksGroup(groupId);
  }
  else if (IsVisible() && IsDrawable())
  {
    engine->UpdateUserMarksGroup(groupId, this);
  }

  engine->InvalidateUserMarks();
}

size_t UserMarkContainer::GetUserPointCount() const
{
  return m_userMarks.size();
}

df::UserPointMark const * UserMarkContainer::GetUserPointMark(size_t index) const
{
  return GetUserMark(index);
}

size_t UserMarkContainer::GetUserLineCount() const
{
  return 0;
}

df::UserLineMark const * UserMarkContainer::GetUserLineMark(size_t index) const
{
  UNUSED_VALUE(index);
  ASSERT(false, ());
  return nullptr;
}

float UserMarkContainer::GetPointDepth() const
{
  return static_cast<float>(m_layerDepth);
}

bool UserMarkContainer::IsVisible() const
{
  return m_flags[VisibleFlag];
}

bool UserMarkContainer::IsDrawable() const
{
  return m_flags[DrawableFlag];
}

UserMark * UserMarkContainer::CreateUserMark(m2::PointD const & ptOrg)
{
  // Push front an user mark.
  SetDirty();
  m_userMarks.push_front(unique_ptr<UserMark>(AllocateUserMark(ptOrg)));
  m_createdMarks.m_marksID.push_back(m_userMarks.front()->GetId());
  return m_userMarks.front().get();
}

size_t UserMarkContainer::GetUserMarkCount() const
{
  return GetUserPointCount();
}

UserMark const * UserMarkContainer::GetUserMark(size_t index) const
{
  ASSERT_LESS(index, m_userMarks.size(), ());
  return m_userMarks[index].get();
}

UserMark::Type UserMarkContainer::GetType() const
{
  return m_type;
}

UserMark * UserMarkContainer::GetUserMarkForEdit(size_t index)
{
  SetDirty();
  ASSERT_LESS(index, m_userMarks.size(), ());
  return m_userMarks[index].get();
}

void UserMarkContainer::Clear()
{
  SetDirty();
  m_userMarks.clear();
}

void UserMarkContainer::SetIsDrawable(bool isDrawable)
{
  if (IsDrawable() != isDrawable)
  {
    SetDirty();
    m_flags[DrawableFlag] = isDrawable;
  }
}

void UserMarkContainer::SetIsVisible(bool isVisible)
{
  if (IsVisible() != isVisible)
  {
    SetDirty();
    m_flags[VisibleFlag] = isVisible;
  }
}

void UserMarkContainer::Update()
{
  SetDirty();
}

void UserMarkContainer::SetDirty()
{
  m_isDirty = true;
}

bool UserMarkContainer::IsDirty() const
{
  return m_isDirty;
}

void UserMarkContainer::DeleteUserMark(size_t index)
{
  SetDirty();
  ASSERT_LESS(index, m_userMarks.size(), ());
  if (index < m_userMarks.size())
  {
    m_removedMarks.m_marksID.push_back(m_userMarks[index]->GetId());
    m_userMarks.erase(m_userMarks.begin() + index);
  }
  else
  {
    LOG(LWARNING, ("Trying to delete non-existing item at index", index));
  }
}

void UserMarkContainer::AcceptChanges(df::MarkIDCollection & createdMarks,
                                      df::MarkIDCollection & removedMarks)
{
  std::swap(m_createdMarks, createdMarks);
  m_createdMarks.Clear();

  std::swap(m_removedMarks, removedMarks);
  m_removedMarks.Clear();

  m_isDirty = false;
}
