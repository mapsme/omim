#pragma once

#include "platform/location.hpp"

#include <QtGui/QMouseEvent>

namespace qt
{
namespace common
{
bool IsLeftButton(Qt::MouseButtons buttons);
bool IsLeftButton(QMouseEvent const * const e);

bool IsRightButton(Qt::MouseButtons buttons);
bool IsRightButton(QMouseEvent const * const e);

bool IsCommandModifier(QMouseEvent const * const e);
bool IsShiftModifier(QMouseEvent const * const e);
bool IsAltModifier(QMouseEvent const * const e);

struct Hotkey
{
  Hotkey() = default;
  Hotkey(int key, char const * slot) : m_key(key), m_slot(slot) {}

  int m_key = 0;
  char const * m_slot = nullptr;
};

location::GpsInfo MakeGpsInfo(m2::PointD const & point);
}  // namespace common
}  // namespace qt
