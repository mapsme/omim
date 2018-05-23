#include "geometry/spline.hpp"

#include "base/logging.hpp"

#include "std/numeric.hpp"

namespace m2
{

Spline::Spline(vector<PointD> const & path)
{
  ASSERT(path.size() > 1, ("Wrong path size!"));
  m_position.assign(path.begin(), path.end());
  size_t cnt = m_position.size() - 1;
  m_direction = vector<PointD>(cnt);
  m_length = vector<double>(cnt);

  for(size_t i = 0; i < cnt; ++i)
  {
    m_direction[i] = path[i + 1] - path[i];
    m_length[i] = m_direction[i].Length();
    m_direction[i] = m_direction[i].Normalize();
  }
}

Spline::Spline(size_t reservedSize)
{
  ASSERT_LESS(0, reservedSize, ());
  m_position.reserve(reservedSize);
  m_direction.reserve(reservedSize - 1);
  m_length.reserve(reservedSize - 1);
}

void Spline::AddPoint(PointD const & pt)
{
  /// TODO remove this check when fix generator.
  /// Now we have line objects with zero length segments
  /// https://jira.mail.ru/browse/MAPSME-3561
  if (!IsEmpty() && (pt - m_position.back()).IsAlmostZero())
    return;

  if(IsEmpty())
    m_position.push_back(pt);
  else
  {
    PointD dir = pt - m_position.back();
    m_position.push_back(pt);
    m_length.push_back(dir.Length());
    m_direction.push_back(dir.Normalize());
  }
}

void Spline::ReplacePoint(PointD const & pt)
{
  ASSERT(m_position.size() > 1, ());
  ASSERT(!m_length.empty(), ());
  ASSERT(!m_direction.empty(), ());
  m_position.pop_back();
  m_length.pop_back();
  m_direction.pop_back();
  AddPoint(pt);
}

bool Spline::IsPrelonging(PointD const & pt)
{
  if (m_position.size() < 2)
    return false;

  PointD dir = pt - m_position.back();
  if (dir.IsAlmostZero())
    return true;

  dir = dir.Normalize();
  PointD prevDir = m_direction.back().Normalize();

  double const MAX_ANGLE_THRESHOLD = 0.995;
  return fabs(DotProduct(prevDir, dir)) > MAX_ANGLE_THRESHOLD;
}

size_t Spline::GetSize() const
{
  return m_position.size();
}

bool Spline::IsEmpty() const
{
  return m_position.empty();
}

bool Spline::IsValid() const
{
  return m_position.size() > 1;
}

Spline::iterator Spline::GetPoint(double step) const
{
  iterator it;
  it.Attach(*this);
  it.Advance(step);
  return it;
}

Spline const & Spline::operator = (Spline const & spl)
{
  if(&spl != this)
  {
    m_position = spl.m_position;
    m_direction = spl.m_direction;
    m_length = spl.m_length;
  }
  return *this;
}

double Spline::GetLength() const
{
  return accumulate(m_length.begin(), m_length.end(), 0.0);
}

void Spline::Clear()
{
  m_position.clear();
  m_direction.clear();
  m_length.clear();
}

Spline::iterator::iterator()
  : m_checker(false)
  , m_spl(NULL)
  , m_index(0)
  , m_dist(0) {}

Spline::iterator::iterator(Spline::iterator const & other)
{
  m_checker = other.m_checker;
  m_spl = other.m_spl;
  m_index = other.m_index;
  m_dist = other.m_dist;

  m_pos = other.m_pos;
  m_dir = other.m_dir;
  m_avrDir = other.m_avrDir;
}

Spline::iterator & Spline::iterator::operator=(Spline::iterator const & other)
{
  if (this == &other)
    return *this;

  m_checker = other.m_checker;
  m_spl = other.m_spl;
  m_index = other.m_index;
  m_dist = other.m_dist;

  m_pos = other.m_pos;
  m_dir = other.m_dir;
  m_avrDir = other.m_avrDir;
  return *this;
}

void Spline::iterator::Attach(Spline const & spl)
{
  m_spl = &spl;
  m_index = 0;
  m_dist = 0;
  m_checker = false;
  m_dir = m_spl->m_direction[m_index];
  m_avrDir = m_spl->m_direction[m_index];
  m_pos = m_spl->m_position[m_index] + m_dir * m_dist;
}

bool Spline::iterator::IsAttached() const
{
  return m_spl != nullptr;
}

void Spline::iterator::Advance(double step)
{
  if (step < 0.0)
    AdvanceBackward(step);
  else
    AdvanceForward(step);
}

double Spline::iterator::GetLength() const
{
  return accumulate(m_spl->m_length.begin(), m_spl->m_length.begin() + m_index, m_dist);
}

double Spline::iterator::GetFullLength() const
{
  return m_spl->GetLength();
}

bool Spline::iterator::BeginAgain() const
{
  return m_checker;
}

double Spline::iterator::GetDistance() const
{
  return m_dist;
}

size_t Spline::iterator::GetIndex() const
{
  return m_index;
}

void Spline::iterator::AdvanceBackward(double step)
{
  m_dist += step;
  while(m_dist < 0.0f)
  {
    if (m_index == 0)
    {
      m_checker = true;
      m_pos = m_spl->m_position[m_index];
      m_dir = m_spl->m_direction[m_index];
      m_avrDir = m2::PointD::Zero();
      m_dist = 0.0;
      return;
    }

    --m_index;

    m_dist += m_spl->m_length[m_index];
  }
  m_dir = m_spl->m_direction[m_index];
  m_avrDir = -m_pos;
  m_pos = m_spl->m_position[m_index] + m_dir * m_dist;
  m_avrDir += m_pos;
}

void Spline::iterator::AdvanceForward(double step)
{
  m_dist += step;
  if (m_checker)
  {
    m_pos = m_spl->m_position[m_index] + m_dir * m_dist;
    return;
  }
  while (m_dist > m_spl->m_length[m_index])
  {
    m_dist -= m_spl->m_length[m_index];
    m_index++;
    if (m_index >= m_spl->m_direction.size())
    {
      m_index--;
      m_dist += m_spl->m_length[m_index];
      m_checker = true;
      break;
    }
  }
  m_dir = m_spl->m_direction[m_index];
  m_avrDir = -m_pos;
  m_pos = m_spl->m_position[m_index] + m_dir * m_dist;
  m_avrDir += m_pos;
}

SharedSpline::SharedSpline(vector<PointD> const & path)
{
  m_spline.reset(new Spline(path));
}

SharedSpline::SharedSpline(SharedSpline const & other)
{
  if (this != &other)
    m_spline = other.m_spline;
}

SharedSpline const & SharedSpline::operator= (SharedSpline const & spl)
{
  if (this != &spl)
    m_spline = spl.m_spline;
  return *this;
}

bool SharedSpline::IsNull() const
{
  return m_spline == NULL;
}

void SharedSpline::Reset(Spline * spline)
{
  m_spline.reset(spline);
}

void SharedSpline::Reset(vector<PointD> const & path)
{
  m_spline.reset(new Spline(path));
}

Spline::iterator SharedSpline::CreateIterator() const
{
  Spline::iterator result;
  result.Attach(*m_spline.get());
  return result;
}

Spline * SharedSpline::operator->()
{
  ASSERT(!IsNull(), ());
  return m_spline.get();
}

Spline const * SharedSpline::operator->() const
{
  return Get();
}

Spline const * SharedSpline::Get() const
{
  ASSERT(!IsNull(), ());
  return m_spline.get();
}
}

