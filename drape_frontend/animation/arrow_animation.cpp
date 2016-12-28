#include "arrow_animation.hpp"

namespace  df
{

ArrowAnimation::ArrowAnimation(m2::PointD const & startPos, m2::PointD const & endPos, double moveDuration,
               double startAngle, double endAngle)
  : Animation(true /* couldBeInterrupted */, true /* couldBeBlended */)
  , m_positionInterpolator(moveDuration, 0.0 /* delay */, startPos, endPos)
  , m_angleInterpolator(startAngle, endAngle)
{
  m_objects.insert(Animation::Object::MyPositionArrow);
  if (m_positionInterpolator.IsActive())
    m_properties.insert(Animation::ObjectProperty::Position);
  if (m_angleInterpolator.IsActive())
    m_properties.insert(Animation::ObjectProperty::Angle);
}

void ArrowAnimation::Init(ScreenBase const & screen, TPropertyCache const & properties)
{
  PropertyValue value;
  if (GetCachedProperty(properties, Animation::Object::MyPositionArrow, Animation::ObjectProperty::Position, value))
  {
    m_positionInterpolator = PositionInterpolator(m_positionInterpolator.GetDuration(),
                                                  0.0 /* delay */,
                                                  value.m_valuePointD,
                                                  m_positionInterpolator.GetTargetPosition());
    if (m_positionInterpolator.IsActive())
      m_properties.insert(Animation::ObjectProperty::Position);
  }
  if (GetCachedProperty(properties, Animation::Object::MyPositionArrow, Animation::ObjectProperty::Angle, value))
  {
    m_angleInterpolator = AngleInterpolator(value.m_valueD, m_angleInterpolator.GetTargetAngle());
    if (m_angleInterpolator.IsActive())
      m_properties.insert(Animation::ObjectProperty::Angle);
  }
}

Animation::TAnimObjects const & ArrowAnimation::GetObjects() const
{
   return m_objects;
}

bool ArrowAnimation::HasObject(Object object) const
{
  return object == Animation::Object::MyPositionArrow;
}

Animation::TObjectProperties const & ArrowAnimation::GetProperties(Object object) const
{
  return m_properties;
}

bool ArrowAnimation::HasProperty(Object object, ObjectProperty property) const
{
  return HasObject(object) && m_properties.find(property) != m_properties.end();
}

void ArrowAnimation::Advance(double elapsedSeconds)
{
  if (m_positionInterpolator.IsActive())
    m_positionInterpolator.Advance(elapsedSeconds);

  if (m_angleInterpolator.IsActive())
    m_angleInterpolator.Advance(elapsedSeconds);
}

void ArrowAnimation::Finish()
{
  if (m_positionInterpolator.IsActive())
    m_positionInterpolator.Finish();

  if (m_angleInterpolator.IsActive())
    m_angleInterpolator.Finish();
}

void ArrowAnimation::SetMaxDuration(double maxDuration)
{
  if (m_positionInterpolator.IsActive())
    m_positionInterpolator.SetMaxDuration(maxDuration);

  if (m_angleInterpolator.IsActive())
    m_angleInterpolator.SetMaxDuration(maxDuration);
}

double ArrowAnimation::GetDuration() const
{
  double duration = 0.0;
  if (m_angleInterpolator.IsActive())
    duration = m_angleInterpolator.GetDuration();
  if (m_positionInterpolator.IsActive())
    duration = max(duration, m_positionInterpolator.GetDuration());
  return duration;
}

bool ArrowAnimation::IsFinished() const
{
  return m_positionInterpolator.IsFinished() && m_angleInterpolator.IsFinished();
}

bool ArrowAnimation::GetProperty(Object object, ObjectProperty property, PropertyValue & value) const
{
  return GetProperty(object, property, false /* targetValue */, value);
}

bool ArrowAnimation::GetTargetProperty(Object object, ObjectProperty property, PropertyValue & value) const
{
  return GetProperty(object, property, true /* targetValue */, value);
}

bool ArrowAnimation::GetProperty(Object object, ObjectProperty property, bool targetValue, PropertyValue & value) const
{
  ASSERT_EQUAL(static_cast<int>(object), static_cast<int>(Animation::Object::MyPositionArrow), ());

  switch (property)
  {
  case Animation::ObjectProperty::Position:
    if (m_positionInterpolator.IsActive())
    {
      value = PropertyValue(targetValue ? m_positionInterpolator.GetTargetPosition() : m_positionInterpolator.GetPosition());
      return true;
    }
    return false;
  case Animation::ObjectProperty::Angle:
    if (m_angleInterpolator.IsActive())
    {
      value = PropertyValue(targetValue ? m_angleInterpolator.GetTargetAngle() : m_angleInterpolator.GetAngle());
      return true;
    }
    return false;
  default:
    ASSERT(false, ("Wrong property:", static_cast<int>(property)));
  }

  return false;
}


} // namespace df
