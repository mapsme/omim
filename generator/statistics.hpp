#pragma once

#include "indexer/feature.hpp"

#include <cstdint>
#include <map>
#include <string>

namespace stats
{
  struct GeneralInfo
  {
    uint64_t m_count, m_size;
    double m_length, m_area;

    GeneralInfo() : m_count(0), m_size(0), m_length(0), m_area(0) {}

    void Add(uint64_t szBytes, double len = 0, double area = 0)
    {
      if (szBytes > 0)
      {
        ++m_count;
        m_size += szBytes;
        m_length += len;
        m_area += area;
      }
    }
  };

  template <class T, int Tag>
  struct IntegralType
  {
    T m_val;
    explicit IntegralType(T v) : m_val(v) {}
    bool operator<(IntegralType const & rhs) const { return m_val < rhs.m_val; }
  };

  using ClassifType = IntegralType<uint32_t, 0>;
  using CountType = IntegralType<uint32_t, 1>;
  using AreaType = IntegralType<size_t, 2>;

  struct MapInfo
  {
    std::map<feature::EGeomType, GeneralInfo> m_byGeomType;
    std::map<ClassifType, GeneralInfo> m_byClassifType;
    std::map<CountType, GeneralInfo> m_byPointsCount, m_byTrgCount;
    std::map<AreaType, GeneralInfo> m_byAreaSize;

    GeneralInfo m_inner[3];
  };

  void FileContainerStatistic(std::string const & fPath);

  void CalcStatistic(std::string const & fPath, MapInfo & info);
  void PrintStatistic(MapInfo & info);
  void PrintTypeStatistic(MapInfo & info);
}
