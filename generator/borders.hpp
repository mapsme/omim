#pragma once

#include "storage/storage_defines.hpp"

#include "coding/geometry_coding.hpp"
#include "coding/reader.hpp"

#include "geometry/rect2d.hpp"
#include "geometry/region2d.hpp"
#include "geometry/tree4d.hpp"

#include <string>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#define BORDERS_DIR "borders/"
#define BORDERS_EXTENSION ".poly"

namespace borders
{
// The raw borders that we have somehow obtained (probably downloaded from
// the OSM and then manually tweaked, but the exact knowledge is lost) are
// stored in the BORDERS_DIR.
// These are the sources of the mwm borders: each file there corresponds
// to exactly one mwm and all mwms except for World and WorldCoasts must
// have a borders file to be generated from.
//
// The file format for raw borders is described at
//   https://wiki.openstreetmap.org/wiki/Osmosis/Polygon_Filter_File_Format
//
// The borders for all mwm files are shipped with the appilication in
// the mwm binary format for geometry data (see coding/geometry_coding.hpp).
// However, storing every single point turned out to take too much space,
// therefore the borders are simplified. This simplification may lead to
// unwanted consequences (for example, empty spaces may occur between mwms)
// but currently we do not take any action against them.

using Region = m2::RegionD;
using RegionsContainer = m4::Tree<Region>;

class CountryPolygons
{
public:
  CountryPolygons() = default;
  explicit CountryPolygons(std::string const & name, RegionsContainer const & regions)
    : m_name(name)
    , m_regions(regions)
  {
    m_regions.ForEach([&](auto const & region) {
      m_rect.Add(region.GetRect());
    });

    auto const innerCells = GetInnerCells();
    for (auto const & cell : innerCells)
      m_innerRects.Add(cell, cell);
  }

  CountryPolygons(CountryPolygons && other) = default;
  CountryPolygons(CountryPolygons const & other) = default;

  CountryPolygons & operator=(CountryPolygons && other) = default;
  CountryPolygons & operator=(CountryPolygons const & other) = default;

  std::string const & GetName() const { return m_name; }
  bool IsEmpty() const { return m_regions.IsEmpty(); }
  void Clear()
  {
    m_regions.Clear();
    m_name.clear();
  }

  bool Contains(m2::PointD const & point) const
  {
    if (m_innerRects.ForAnyInRect(m2::RectD(point, point), [&](auto const &) { return true; }))
      return true;

    return Contains_(point);
  }

private:
  std::vector<m2::RectD> MakeCells(size_t count)
  {
    std::vector<m2::RectD> cells;

    auto const minLen = std::min(m_rect.SizeX(), m_rect.SizeY());
    auto const step = minLen / count;

    double currY1 = m_rect.minY();
    double currY2 = m_rect.minY() + step;
    while (currY1 <= m_rect.maxY())
    {
      double currX1 = m_rect.minX();
      double currX2 = m_rect.minX() + step;
      while (currX2 <= m_rect.maxX())
      {
        m2::RectD cell;
        cell.setMinX(currX1);
        cell.setMaxX(currX2);
        cell.setMinY(currY1);
        cell.setMaxY(currY2);
        cells.emplace_back(cell);
        currX1 = currX2;
        currX2 += step;
      }
      currY1 = currY2;
      currY2 += step;
    }

    return cells;
  }

  std::vector<m2::RectD> GetInnerCells()
  {
    std::vector<m2::RectD> innerCells;
    auto const cells = MakeCells(16/* count */);
    for (auto const & cell : cells)
    {
      if (Contains_(cell.LeftBottom()) &&
          Contains_(cell.LeftTop()) &&
          Contains_(cell.RightBottom()) &&
          Contains_(cell.RightTop()))
      {
        innerCells.emplace_back(cell);
      }
    }
    return innerCells;
  }

  bool Contains_(m2::PointD const & point) const
  {
    return m_regions.ForAnyInRect(m2::RectD(point, point), [&](auto const & rgn) {
      return rgn.Contains(point);
    });
  }

  std::string m_name;
  RegionsContainer m_regions;
  m2::RectD m_rect;
  m4::Tree<m2::RectD> m_innerRects;
};

class CountriesContainer
{
public:
  CountriesContainer() = default;
  explicit CountriesContainer(m4::Tree<CountryPolygons> const & tree)
    : m_regionsTree(tree)
  {
    tree.ForEach([&](auto const & region) {
      m_regions.emplace(region.GetName(), region);
    });
  }

  template <typename ToDo>
  void ForEachInRect(m2::RectD const & rect, ToDo && toDo) const
  {
    m_regionsTree.ForEachInRect(rect, std::forward<ToDo>(toDo));
  }

  bool HasRegionByName(std::string const & name) const
  {
    return m_regions.count(name) != 0;
  }

  CountryPolygons const & GetRegionByName(std::string const & name) const
  {
    return m_regions.at(name);
  }

private:
  m4::Tree<CountryPolygons> m_regionsTree;
  std::unordered_map<std::string, CountryPolygons> m_regions;
};

/// @return false if borderFile can't be opened
bool LoadBorders(std::string const & borderFile, std::vector<m2::RegionD> & outBorders);

bool GetBordersRect(std::string const & baseDir, std::string const & country,
                    m2::RectD & bordersRect);

bool LoadCountriesList(std::string const & baseDir, CountriesContainer & countries);

void GeneratePackedBorders(std::string const & baseDir);

template <typename Source>
std::vector<m2::RegionD> ReadPolygonsOfOneBorder(Source & src)
{
  auto const count = ReadVarUint<uint32_t>(src);
  std::vector<m2::RegionD> result(count);
  for (size_t i = 0; i < count; ++i)
  {
    std::vector<m2::PointD> points;
    serial::LoadOuterPath(src, serial::GeometryCodingParams(), points);
    result[i] = m2::RegionD(std::move(points));
  }

  return result;
}

void DumpBorderToPolyFile(std::string const & filePath, storage::CountryId const & mwmName,
                          std::vector<m2::RegionD> const & polygons);
void UnpackBorders(std::string const & baseDir, std::string const & targetDir);

class PackedBorders
{
public:
  static std::shared_ptr<CountriesContainer> GetOrCreate(std::string const & name);

private:
  static std::mutex m_mutex;
  static std::unordered_map<std::string, std::shared_ptr<CountriesContainer>> m_countries;
};
}  // namespace borders
