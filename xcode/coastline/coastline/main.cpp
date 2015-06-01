//
//  main.cpp
//  coastline
//
//  Created by Sergey Yershov on 28.05.15.
//  Copyright (c) 2015 maps.me. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <deque>
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>

#define BOOST_GEOMETRY_DEBUG_SEGMENT_IDENTIFIER
#define BOOST_GEOMETRY_DEBUG_HAS_SELF_INTERSECTIONS
//#define BOOST_GEOMETRY_OVERLAY_NO_THROW
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/algorithms/detail/overlay/turn_info.hpp>
#include <boost/geometry/extensions/algorithms/dissolve.hpp>


namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

typedef bg::model::d2::point_xy<double> point;
typedef bg::model::box<point> box;
typedef bg::model::polygon<point> polygon;
typedef std::pair<box, size_t> value;
typedef bg::model::ring<point> ring;

typedef boost::geometry::model::multi_polygon<polygon, std::deque> multi_polygon;


namespace m2 {
struct PointD {double x; double y;};
struct PointU {uint32_t x; uint32_t y;};
}

namespace my
{
  template <typename TFloat> inline TFloat DegToRad(TFloat deg)
  {
    return deg * TFloat(3.1415926536) / TFloat(180);
  }

  template <typename TFloat> inline TFloat RadToDeg(TFloat rad)
  {
    return rad * TFloat(180) / TFloat(3.1415926536);
  }
}

m2::PointD PointU2PointD(m2::PointU const & pt, uint32_t coordBits)
{
  double x = static_cast<double>(pt.x) * (360.0) / ((1 << coordBits) - 1) + -180;
  double y = static_cast<double>(pt.y) * (360.0) / ((1 << coordBits) - 1) + -180;
  return {x,y};
}

double YToLat(double y)
{
  return my::RadToDeg(2.0 * atan(tanh(0.5 * my::DegToRad(y))));
}

using std::function;
using std::string;
using std::thread;
using std::stringstream;
using std::ofstream;
namespace this_thread = std::this_thread;
#define POINT_COORD_BITS 30


std::ostream & operator << (std::ostream & s, multi_polygon const & multipolygon)
{
  size_t ringNum = 1;
  s << "multipolygon_" << multipolygon.size() << std::endl;
  for (auto const & p : multipolygon)
  {
    s << ringNum++ << std::endl;
    for(auto const & op : p.outer())
      s << std::fixed << std::setprecision(7) << op.x() << " " << op.y() << std::endl;
    s << "END" << std::endl;
    for(auto const & ring : p.inners())
    {
      s << "!" << ringNum++ << (ring.size() < 3 ? "invalid" : "") << std::endl;
      for(auto const & op : ring)
        s << std::fixed << std::setprecision(7) << op.x() << " " << op.y() << std::endl;
      s << "END" << std::endl;
    }
  }
  s << "END" << std::endl;
  return s;
}

void SavePoly(std::string const & name, multi_polygon const & poly)
{
  ofstream file(name.c_str());
  file << poly << std::endl;
}

class RegionMill
{
  typedef bgi::rtree< value, bgi::quadratic<16>> TIndex;
  typedef std::deque<polygon> TContainer;

  TContainer const & m_container;
  TIndex const & m_index;

  typedef function<void(string const &, multi_polygon &)> TProcessResultFunc;

  TProcessResultFunc m_processResultFunc;

  struct Cell
  {
    enum class EStage {Init, NeedSplit, Done} stage;
    size_t id;
    std::string suffix;
    box bounds;
    multi_polygon geom;
  };

  struct PolygonDesc
  {
    size_t level;
    std::vector<size_t> covered;
  };

  std::mutex & m_mutexTasks;
  std::list<Cell> & m_listTasks;
  std::condition_variable & m_listCondVar;
  size_t & m_inWork;
  std::mutex & m_mutexResult;

  enum { kMaxPointsInCell = 20000, kMaxRegionsInCell = 500 };


public:
  static void Process(uint32_t numThreads, uint32_t baseScale, std::deque<polygon> const & polygons, TProcessResultFunc funcResult)
  {
    TIndex rtree;

    for (size_t i = 0; i < polygons.size(); ++i)
      rtree.insert(value(bg::return_envelope<box>(polygons[i]), i));

    std::list<RegionMill::Cell> listTasks = RegionMill::CreateCellsForScale(baseScale, 721, 1);
    std::list<multi_polygon> listResult;

    std::vector<RegionMill> uniters;
    std::vector<std::thread> threads;
    std::mutex mutexTasks;
    std::mutex mutexResult;
    std::condition_variable condVar;
    size_t inWork = 0;
    for (uint32_t i = 0; i < numThreads; ++i)
    {
      uniters.emplace_back(RegionMill(listTasks, mutexTasks, condVar, inWork, funcResult, mutexResult, polygons, rtree));
      threads.emplace_back(thread(uniters.back()));
    }

    for (auto & thread : threads)
      thread.join();
  }

protected:

  RegionMill(std::list<Cell> & listTasks, std::mutex & mutexTasks, std::condition_variable & condVar, size_t & inWork,
             TProcessResultFunc funcResult, std::mutex & mutexResult,
             TContainer const &container, TIndex const & index)
  : m_container(container)
  , m_index(index)
  , m_processResultFunc(funcResult)
  , m_mutexTasks(mutexTasks)
  , m_listTasks(listTasks)
  , m_listCondVar(condVar)
  , m_inWork(inWork)
  , m_mutexResult(mutexResult)
  {}

  static std::list<Cell> CreateCellsForScale(uint32_t scale, uint32_t from = 0, uint32_t num = 0)
  {
    uint32_t numCells = 1 << scale;
    uint32_t cellSize = ((1 << POINT_COORD_BITS) - 1) / numCells;

    std::list<Cell> result;

    uint32_t fromCell = from;
    uint32_t toCell = (num == 0) ? (numCells * numCells) : fromCell + num;

    for (uint32_t i = fromCell; i < toCell; ++i)
    {
      uint32_t xCell = i % numCells;
      uint32_t yCell = i / numCells;
      m2::PointD minPoint = PointU2PointD(m2::PointU{(xCell * cellSize), (yCell * cellSize)}, POINT_COORD_BITS);
      m2::PointD maxPoint = PointU2PointD(m2::PointU{(xCell * cellSize + cellSize), (yCell * cellSize + cellSize)}, POINT_COORD_BITS);

      box cell(point(minPoint.x, YToLat(minPoint.y)) , point(maxPoint.x, YToLat(maxPoint.y)));
      result.push_back({Cell::EStage::Init, i, "", cell, {}});
    }
    return result;
  }

  void SplitCell(Cell const & cell, std::vector<Cell> & outCells)
  {
    //  +-----+-----+
    //  |     |     |
    //  |  2  |  3  |
    //  |     |     |
    //  +-----+-----+
    //  |     |     |
    //  |  0  |  1  |
    //  |     |     |
    //  +-----+-----+
    //
    double middleX = (cell.bounds.min_corner().x() + cell.bounds.max_corner().x()) / 2;
    double middleY = (cell.bounds.min_corner().y() + cell.bounds.max_corner().y()) / 2;

    outCells.resize(4);
    outCells[0].bounds = cell.bounds;
    outCells[0].bounds.max_corner() = point(middleX, middleY);
    outCells[1].bounds = cell.bounds;
    outCells[1].bounds.min_corner().x(middleX);
    outCells[1].bounds.max_corner().y(middleY);
    outCells[2].bounds = cell.bounds;
    outCells[2].bounds.min_corner().y(middleY);
    outCells[2].bounds.max_corner().x(middleX);
    outCells[3].bounds = cell.bounds;
    outCells[3].bounds.min_corner() = point(middleX, middleY);

    for (size_t i = 0; i < outCells.size(); ++i)
    {
      outCells[i].id = cell.id;
      outCells[i].suffix = cell.suffix + char('0'+i);
    }
  }

  void MakeCellGeometry(Cell const & src, std::vector<Cell> & result)
  {
    auto start = std::chrono::system_clock::now();
    multi_polygon poly;
    result.push_back(src);

    Cell & task = result.front();
    // fetch regions from R-Tree by cell bounds
    for (auto it = m_index.qbegin(bgi::intersects(task.bounds)); it != m_index.qend(); ++it)
      poly.emplace_back(m_container[it->second]);

    if (poly.size() > kMaxRegionsInCell)
    {
      SplitCell(src, result);
      for (Cell & cell : result)
        cell.stage = Cell::EStage::Init;
      return;
    }

    std::cout << this_thread::get_id() << " cell: " << task.id << "-" << task.suffix << " polygons: " << poly.size()
    << " points: " << bg::num_points(poly) << std::endl;

    // arrange rings in right order
    std::map<size_t, PolygonDesc> order;
    for (size_t i = 0; i < poly.size(); ++i)
    {
      order[i].level = 0;
      for (size_t j = 0; j < poly.size(); ++j)
      {
        if (i == j)
          continue;

        if (bg::within(poly[i].outer()[0], poly[j])) {
          order[i].covered.push_back(j);
          order[i].level++;
        }
      }
    }

    for (auto & it : order)
      it.second.covered.erase(std::remove_if(it.second.covered.begin(), it.second.covered.end(), [it, &order](size_t const & el){return (order[el].level != it.second.level-1);}), it.second.covered.end());

    for (size_t i = 0; i < poly.size(); ++i)
    {
      // odd order
      if (order[i].level & 0x01)
      {
        poly[order[i].covered.front()].inners().emplace_back(std::move(poly[i].outer()));
        poly[i].outer().clear();
      }
    }

    poly.erase(std::remove_if(poly.begin(), poly.end(), [](polygon const &p){ return (bg::num_points(p) == 0);}), poly.end());
    for (size_t i = 0; i < poly.size(); ++i)
      bg::correct(poly[i]);
    auto stage1 = std::chrono::system_clock::now();

    // clip regions by cell bounds and invert it
    boost::geometry::intersection(task.bounds, poly, task.geom);
    poly.swap(task.geom);
    bg::clear(task.geom);
    boost::geometry::difference(task.bounds, poly, task.geom);

    task.stage = (bg::num_points(task.geom) > kMaxPointsInCell) ? Cell::EStage::NeedSplit : Cell::EStage::Done;

    auto stage2 = std::chrono::system_clock::now();

    stringstream sss;
    sss << this_thread::get_id() << " cell: " << task.id << "-" << task.suffix << " polygons: " << task.geom.size()
    << " points: " << bg::num_points(task.geom) << " create poly: "
    << std::fixed << std::setprecision(7) << std::chrono::duration<double>(stage1 - start).count() << " sec."
    << " make cell : " << std::chrono::duration<double>(stage2 - stage1).count() << " sec.";
//    LOG(LINFO, (sss.str()));
    std::cout << sss.str() << std::endl;
  }

  void SplitCellGeometry(Cell const & cell, std::vector<Cell> & outCells)
  {
    auto start = std::chrono::system_clock::now();
    SplitCell(cell, outCells);

    for (size_t i = 0; i < outCells.size(); ++i)
    {
      boost::geometry::intersection(outCells[i].bounds, cell.geom, outCells[i].geom);
      outCells[i].stage = (bg::num_points(outCells[i].geom) > kMaxPointsInCell) ? Cell::EStage::NeedSplit : Cell::EStage::Done;
    }
    auto end = std::chrono::system_clock::now();

    stringstream ss;
    ss << this_thread::get_id() << " Split cell: " << cell.id << "-" << cell.suffix << " polygons: " << cell.geom.size()
    << " points: " << bg::num_points(cell.geom);
    ss << " done in " << std::fixed << std::setprecision(7) << std::chrono::duration<double>(end - start).count() << " sec.";
//    LOG(LINFO, (ss.str()));
    std::cout << ss.str() << std::endl;
  }

  void ProcessCell(Cell const & cell, std::vector<Cell> & outCells)
  {
    auto start = std::chrono::system_clock::now();
    switch (cell.stage)
    {
      case Cell::EStage::Init:
        MakeCellGeometry(cell, outCells);
        break;
      case Cell::EStage::NeedSplit:
        SplitCellGeometry(cell, outCells);
        break;
      default:
        break;
    }
    auto end = std::chrono::system_clock::now();

    // store ready result
    for (auto & cell : outCells)
    {
      if (cell.stage == Cell::EStage::Done && bg::num_points(cell.geom) != 0)
      {
//        uncomment if need debug result polygon
//        std::stringstream fname;
//        fname << "./cell" << std::setfill('0') << std::setw(8) << cell.id << cell.suffix << ".poly";
//        ofstream file(fname.str().c_str());
//        file << cell.geom << std::endl;
        stringstream name;
        name << cell.id << cell.suffix;
        std::lock_guard<std::mutex> lock(m_mutexResult);
        m_processResultFunc(name.str(), cell.geom);
      }
    }
    stringstream ss;
    ss << this_thread::get_id() << " cell: " << cell.id << "-" << cell.suffix;
    ss << " processed in " << std::fixed << std::setprecision(7) << std::chrono::duration<double>(end - start).count() << " sec.";
    //        LOG(LINFO, (ss.str()));
    std::cout << ss.str() << std::endl;
  }


public:
  void operator()()
  {
    for (;;)
    {

      Cell currentCell;
      std::unique_lock<std::mutex> lock(m_mutexTasks);
      m_listCondVar.wait(lock, [this]{return (!m_listTasks.empty() || m_inWork == 0);});
      if (m_listTasks.empty() && m_inWork == 0)
      {
        std::cout << this_thread::get_id() << " thread exit" << std::endl;
        return;
      }
      currentCell = m_listTasks.front();
      m_listTasks.pop_front();
      ++m_inWork;
      lock.unlock();

      std::vector<Cell> resultCells;

      ProcessCell(currentCell, resultCells);

      lock.lock();
      // return to queue not ready cells
      for (auto const & cell : resultCells)
      {
        if (cell.stage == Cell::EStage::NeedSplit || cell.stage == Cell::EStage::Init)
        {
          m_listTasks.push_back(cell);
        }
      }
      --m_inWork;
      m_listCondVar.notify_all();
    }
  }
};



int main(int argc, const char * argv[]) {
  std::ifstream f("../merged_coastline_double.dump");
//529724

  std::deque<polygon> polygons;

  uint32_t size = 0;
  size_t readed = 0;

  std::time_t timestamp;
  timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::cout << "Start computation at " << std::ctime(&timestamp) << std::endl;

  for(;;)
  {
    f.read((char *)&size, sizeof(uint32_t));

    if (!size)
      break;

    std::vector<m2::PointD> region(size);
    f.read((char *)region.data(), size * sizeof(m2::PointD));
    polygons.push_back(polygon());
    polygons.back().outer().reserve(region.size());
    for (auto const & p : region)
    {
      polygons.back().outer().emplace_back(point{p.x, p.y});
    }
    bg::correct(polygons.back());
    bg::validity_failure_type failureType;
    if (!bg::is_valid(polygons.back(), failureType))
    {
      std::cout << "----------------------------------------------------------\n";
      std::cout << "num_points: " << bg::num_points(polygons.back()) << std::endl;

      std::ostringstream message;
      message << std::fixed << std::setprecision(7);
      bg::failing_reason_policy<> policy_visitor(message);
      is_valid(polygons.back(), policy_visitor);

      std::cout << "Problem: " << bg::validity_failure_type_message(failureType) << std::endl;
      std::cout << "Extended problem: " << message.str() << std::endl;
      std::cout << "num: " << readed << " polygon outer size: " << bg::num_points(polygons.back()) << std::endl;
      if (failureType == bg::failure_self_intersections)
      {
        multi_polygon out;
        bg::dissolve(polygons.back(), out);
        std::cout << "After dissolve: " << bg::num_points(out) << " polygons: " << bg::num_geometries(out) << std::endl;
        sort(out.begin(), out.end(), [](polygon const & p1, polygon const & p2) {return bg::num_points(p2) < bg::num_points(p1);});
        bg::assign(polygons.back(), out.front());
      }
      if (failureType == bg::failure_spikes)
      {
        bg::remove_spikes(polygons.back());
        std::cout << "After remove spikes: " << bg::num_points(polygons.back()) << " polygons: " << bg::num_geometries(polygons.back()) << std::endl;
      }
      std::cout << "num_points: " << bg::num_points(polygons.back()) << std::endl;
      std::cout << "----------------------------------------------------------\n";
    }
    readed++;
  }

  timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::cout << "Load " << readed << " done at " << std::ctime(&timestamp) << std::endl;

  uint32_t numThreads = thread::hardware_concurrency();

  // sort by increase num points in polygon
  sort(polygons.begin(), polygons.end(), [](polygon const & p1, polygon const & p2) {return bg::num_points(p1) < bg::num_points(p2);});

  std::cout << "First polygon numpoints: " << bg::num_points(polygons.front()) << std::endl;

  RegionMill::Process(numThreads, 5, polygons, [](string const & name, multi_polygon & resultPolygon)
                      {
                      });

  timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::cout << "Finished computation at " << std::ctime(&timestamp) << std::endl;

  return 0;
}
