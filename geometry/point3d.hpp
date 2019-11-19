#pragma once

#include "base/math.hpp"

#include <cmath>
#include <sstream>
#include <string>

namespace m3
{
template <typename T>
class Point
{
public:
  constexpr Point() : x(T()), y(T()), z(T()) {}
  constexpr Point(T const & x, T const & y, T const & z) : x(x), y(y), z(z) {}

  T Length() const { return std::sqrt(x * x + y * y + z * z); }

  Point RotateAroundX(double angleDegree) const;
  Point RotateAroundY(double angleDegree) const;
  Point RotateAroundZ(double angleDegree) const;

  bool operator==(Point<T> const & rhs) const;

  T x;
  T y;
  T z;
};

template <typename T>
Point<T> Point<T>::RotateAroundX(double angleDegree) const
{
  double const angleRad = base::DegToRad(angleDegree);
  Point<T> res;
  res.x = x;
  res.y = y * cos(angleRad) - z * sin(angleRad);
  res.z = y * sin(angleRad) + z * cos(angleRad);
  return res;
}

template <typename T>
Point<T> Point<T>::RotateAroundY(double angleDegree) const
{
  double const angleRad = base::DegToRad(angleDegree);
  Point<T> res;
  res.x = x * cos(angleRad) + z * sin(angleRad);
  res.y = y;
  res.z = -x * sin(angleRad) + z * cos(angleRad);
  return res;
}

template <typename T>
Point<T> Point<T>::RotateAroundZ(double angleDegree) const
{
  double const angleRad = base::DegToRad(angleDegree);
  Point<T> res;
  res.x = x * cos(angleRad) - y * sin(angleRad);
  res.y = x * sin(angleRad) + y * cos(angleRad);
  res.z = z;
  return res;
}

template <typename T>
bool Point<T>::operator==(m3::Point<T> const & rhs) const
{
  return x == rhs.x && y == rhs.y && z == rhs.z;
}

template <typename T>
T DotProduct(Point<T> const & a, Point<T> const & b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

template <typename T>
Point<T> CrossProduct(Point<T> const & a, Point<T> const & b)
{
  auto const x = a.y * b.z - a.z * b.y;
  auto const y = a.z * b.x - a.x * b.z;
  auto const z = a.x * b.y - a.y * b.x;
  return Point<T>(x, y, z);
}

using PointD = Point<double>;

template <typename T>
std::string DebugPrint(Point<T> const & p)
{
  std::ostringstream out;
  out.precision(20);
  out << "m3::Point<" << typeid(T).name() << ">(" << p.x << ", " << p.y << ", " << p.z << ")";
  return out.str();
}
}  // namespace m3

namespace base
{
template <typename T>
bool AlmostEqualAbs(m3::Point<T> const & p1, m3::Point<T> const & p2, double const & eps)
{
  return base::AlmostEqualAbs(p1.x, p2.x, eps) && base::AlmostEqualAbs(p1.y, p2.y, eps) &&
         base::AlmostEqualAbs(p1.z, p2.z, eps);
}
}  // namespace base
