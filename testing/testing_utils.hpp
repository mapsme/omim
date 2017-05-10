#pragma once
#include <vector>

template <typename T> std::vector<T> Vec(T x0)
{
  std::vector<T> v;
  v.push_back(x0);
  return v;
}

template <typename T> std::vector<T> Vec(T x0, T x1)
{
  std::vector<T> v;
  v.push_back(x0);
  v.push_back(x1);
  return v;
}

template <typename T> std::vector<T> Vec(T x0, T x1, T x2)
{
  std::vector<T> v;
  v.push_back(x0);
  v.push_back(x1);
  v.push_back(x2);
  return v;
}

template <typename T> std::vector<T> Vec(T x0, T x1, T x2, T x3)
{
  std::vector<T> v;
  v.push_back(x0);
  v.push_back(x1);
  v.push_back(x2);
  v.push_back(x3);
  return v;
}
