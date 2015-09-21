#include "generator/coastlines_generator.hpp"
#include "generator/feature_builder.hpp"

#include "indexer/point_to_int64.hpp"

#include "base/logging.hpp"

#include "std/bind.hpp"
#include "std/chrono.hpp"
#include "std/condition_variable.hpp"
#include "std/function.hpp"
#include "std/thread.hpp"
#include "std/utility.hpp"
#include "std/fstream.hpp"
#include "std/functional.hpp"
#include "std/thread.hpp"

#include "boost/geometry.hpp"
#include "boost/geometry/algorithms/detail/overlay/turn_info.hpp"
#include "boost/geometry/extensions/algorithms/dissolve.hpp"
#include "boost/geometry/geometries/point_xy.hpp"

CoastlineFeaturesGenerator::CoastlineFeaturesGenerator(uint32_t coastType)
  : m_merger(POINT_COORD_BITS), m_coastType(coastType)
{}

namespace
{
  class DoCreateDoubleRegion
  {
    m2::PointD m_pt;
    bool m_exist;

  public:
    vector<m2::PointD> m_rgn;
    DoCreateDoubleRegion() : m_exist(false) {}

    bool operator()(m2::PointD const & p)
    {
      // This logic is to skip last polygon point (that is equal to first).
      if (m_exist)
      {
        // add previous point to region
        m_pt.y = MercatorBounds::YToLat(m_pt.y);
        m_rgn.push_back(m_pt);
      }
      else
        m_exist = true;

      // save current point
      m_pt = p;
      return true;
    }

    void EndRegion()
    {
      m_exist = false;
    }
  };

}

void CoastlineFeaturesGenerator::AddRegion(FeatureBuilder1 const & fb)
{
  ASSERT ( fb.IsGeometryClosed(), () );

  DoCreateDoubleRegion region;
  fb.ForEachGeometryPointEx(region);
  m_regions.push_back(region.m_rgn);
}

void CoastlineFeaturesGenerator::operator()(FeatureBuilder1 const & fb)
{
  if (fb.IsGeometryClosed())
    AddRegion(fb);
  else
    m_merger(fb);
}

namespace
{
  class MergedRegionSaver : public FeatureEmitterIFace
  {
    CoastlineFeaturesGenerator & m_rMain;
    size_t m_notMergedCoastsCount;

  public:
    MergedRegionSaver(CoastlineFeaturesGenerator & rMain)
      : m_rMain(rMain), m_notMergedCoastsCount(0) {}

    virtual void operator() (FeatureBuilder1 const & fb)
    {
      if (fb.IsGeometryClosed())
        m_rMain.AddRegion(fb);
      else
      {
        LOG(LINFO, ("Not merged coastline", fb.GetOsmIdsString()));
        ++m_notMergedCoastsCount;
      }
    }

    bool HasNotMergedCoasts() const
    {
      return m_notMergedCoastsCount != 0;
    }

    size_t GetNotMergedCoastsCount() const
    {
      return m_notMergedCoastsCount;
    }
  };
}

void DumpRegionsIntoFile(string const &fileName, list<vector<m2::PointD>> const & regions)
{
  ofstream dumpStream;
  LOG(LINFO, ("Dump double coastsline into:", fileName));
  dumpStream.open(fileName);

  for (auto const & rgn : regions)
  {
    uint32_t sz = static_cast<uint32_t>(rgn.size());
    dumpStream.write((char *)&sz, sizeof(uint32_t));
    dumpStream.write((char *)rgn.data(), rgn.size() * sizeof(m2::PointD));
  }

  uint32_t term = 0;
  dumpStream.write((char *)&term, sizeof(uint32_t));
  dumpStream.flush();
  dumpStream.close();
}

bool CoastlineFeaturesGenerator::Finish(string const & intermediateDir)
{
  MergedRegionSaver saver(*this);
  m_merger.DoMerge(saver);

  if (saver.HasNotMergedCoasts())
  {
    LOG(LINFO, ("Total not merged coasts:", saver.GetNotMergedCoastsCount()));
    return false;
  }

  // dump merged coasts for experimental purposes
  string dumpName = (intermediateDir + "/merged_coastline_double.dump");
  DumpRegionsIntoFile(dumpName, m_regions);

  return true;
}

namespace
{
  namespace bg = boost::geometry;
  namespace bgi = boost::geometry::index;

  typedef bg::model::d2::point_xy<double> point;
  typedef bg::model::box<point> box;
  typedef bg::model::polygon<point> polygon;
  typedef bg::model::multi_polygon<polygon, deque> multi_polygon;
  typedef pair<box, size_t> value;


  class RegionMill
  {
    typedef bgi::rtree< value, bgi::quadratic<16>> TIndex;
    typedef deque<polygon> TContainer;

    TContainer const & m_container;
    TIndex const & m_index;

    typedef function<void(string const &, multi_polygon &)> TProcessResultFunc;

    TProcessResultFunc m_processResultFunc;

    struct Cell
    {
      enum class EStage {Init, NeedSplit, Done} stage;
      size_t id;
      string suffix;
      box bounds;
      multi_polygon geom;
    };

    struct PolygonDesc
    {
      size_t level;
      vector<size_t> covered;
    };

    mutex & m_mutexTasks;
    list<Cell> & m_listTasks;
    list<Cell> m_errorCell;
    condition_variable & m_listCondVar;
    size_t & m_inWork;
    mutex & m_mutexResult;

    enum { kMaxPointsInCell = 20000, kMaxRegionsInCell = 500 };


  public:
    static bool Process(uint32_t numThreads, uint32_t baseScale, deque<polygon> const & polygons, TProcessResultFunc funcResult)
    {
      TIndex rtree;

      for (size_t i = 0; i < polygons.size(); ++i)
        rtree.insert(value(bg::return_envelope<box>(polygons[i]), i));

      list<RegionMill::Cell> listTasks = RegionMill::CreateCellsForScale(baseScale);

      vector<RegionMill> uniters;
      vector<thread> threads;
      mutex mutexTasks;
      mutex mutexResult;
      condition_variable condVar;
      size_t inWork = 0;
      for (uint32_t i = 0; i < numThreads; ++i)
      {
        uniters.emplace_back(RegionMill(listTasks, mutexTasks, condVar, inWork, funcResult, mutexResult, polygons, rtree));
        threads.emplace_back(thread(uniters.back()));
      }

      for (auto & thread : threads)
        thread.join();
      // return true if listTask has no error cells
      return listTasks.empty();
    }

    static bool CorrectAndCheckPolygon(polygon & p)
    {
      bool validPolygon = false;
      // correct orientation (ccw, cw), remove duplicate points
      bg::correct(p);

      bg::validity_failure_type failureType;

      if ((validPolygon = bg::is_valid(p, failureType)))
        return validPolygon;

      ostringstream message;
      message << fixed << setprecision(7);
      bg::failing_reason_policy<> policy_visitor(message);
      is_valid(p, policy_visitor);
      size_t pointsInSourcePolygon = bg::num_points(p);
      LOG(LWARNING, ("Num points:", pointsInSourcePolygon, "Error:", message.str()));

      // correct self intersect geometry
      if (failureType == bg::failure_self_intersections)
      {
        multi_polygon out;
        bg::dissolve(p, out);
        size_t pointsInCorrectedPolygon = bg::num_points(out);
        LOG(LWARNING, ("After dissolve:", pointsInCorrectedPolygon, "polygons:", bg::num_geometries(out)));
        // if we lose more then 20 points for one polygon we sould stop the work
        validPolygon = abs((int)pointsInSourcePolygon - (int)pointsInCorrectedPolygon) < 20;
        // After dissolve we need select bigest result polygon. We sort results and get first polygon.
        sort(out.begin(), out.end(), [](polygon const & p1, polygon const & p2) {return bg::num_points(p2) < bg::num_points(p1);});
        bg::assign(p, out.front());
      }
      if (failureType == bg::failure_spikes)
      {
        bg::remove_spikes(p);
        LOG(LWARNING, ("After remove spikes:", bg::num_points(p), "polygons:", bg::num_geometries(p)));
        validPolygon = true;
      }
      LOG(LWARNING, ("Fixed points:", bg::num_points(p), "polygons:", bg::num_geometries(p)));

      return validPolygon;
    }

  protected:

    RegionMill(list<Cell> & listTasks, mutex & mutexTasks, condition_variable & condVar, size_t & inWork,
               TProcessResultFunc funcResult, mutex & mutexResult,
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

    static list<Cell> CreateCellsForScale(uint32_t scale, uint32_t from = 0, uint32_t num = 0)
    {
      uint32_t numCells = 1 << scale;
      uint32_t cellSize = ((1 << POINT_COORD_BITS) - 1) / numCells;

      list<Cell> result;

      uint32_t fromCell = from;
      uint32_t toCell = (num == 0) ? (numCells * numCells) : fromCell + num;

      for (uint32_t i = fromCell; i < toCell; ++i)
      {
        uint32_t xCell = i % numCells;
        uint32_t yCell = i / numCells;
        m2::PointD minPoint = PointU2PointD(m2::PointU{(xCell * cellSize), (yCell * cellSize)}, POINT_COORD_BITS);
        m2::PointD maxPoint = PointU2PointD(m2::PointU{(xCell * cellSize + cellSize), (yCell * cellSize + cellSize)}, POINT_COORD_BITS);

        box cell(point(minPoint.x, MercatorBounds::YToLat(minPoint.y)) , point(maxPoint.x, MercatorBounds::YToLat(maxPoint.y)));
        result.push_back({Cell::EStage::Init, i, "", cell, {}});
      }
      return result;
    }

    void SplitCell(Cell const & cell, vector<Cell> & outCells)
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

    void MakeCellGeometry(Cell const & src, vector<Cell> & result)
    {
      auto start = system_clock::now();
      multi_polygon poly;
      result.push_back(src);

      Cell & task = result.front();
      // fetch regions from R-Tree by cell bounds
      for (auto it = m_index.qbegin(bgi::intersects(task.bounds)); it != m_index.qend(); ++it)
        poly.emplace_back(m_container[it->second]);

      // split cell if too many polygons in it
      if (poly.size() > kMaxRegionsInCell)
      {
        SplitCell(src, result);
        for (Cell & cell : result)
          cell.stage = Cell::EStage::Init;
        return;
      }

      // arrange rings in right order
      map<size_t, PolygonDesc> order;
      for (size_t i = 0; i < poly.size(); ++i)
      {
        order[i].level = 0;
        for (size_t j = 0; j < poly.size(); ++j)
        {
          if (i == j)
            continue;

          if (bg::covered_by(poly[i].outer()[0], poly[j]))
          {
            order[i].covered.push_back(j);
            order[i].level++;
          }
        }
      }

      for (auto & it : order)
      {
        it.second.covered.erase(remove_if(it.second.covered.begin(), it.second.covered.end(),
                                          [it, &order](size_t const & el)
                                          {return (order[el].level != it.second.level - 1);}),
                                it.second.covered.end());
      }

      for (size_t i = 0; i < poly.size(); ++i)
      {
        // odd order
        if (order[i].level & 0x01)
        {
          poly[order[i].covered.front()].inners().emplace_back(move(poly[i].outer()));
          poly[i].outer().clear();
        }
      }

      poly.erase(remove_if(poly.begin(), poly.end(), [](polygon const & p)
                           {return (bg::num_points(p) == 0);}),poly.end());

      for (size_t i = 0; i < poly.size(); ++i)
        bg::correct(poly[i]);
      auto stage1 = system_clock::now();

      // clip regions by cell bounds and invert it
      boost::geometry::intersection(task.bounds, poly, task.geom);
      poly.swap(task.geom);
      bg::clear(task.geom);
      boost::geometry::difference(task.bounds, poly, task.geom);

      task.stage = (bg::num_points(task.geom) > kMaxPointsInCell) ? Cell::EStage::NeedSplit
                                                                  : Cell::EStage::Done;

      auto stage2 = system_clock::now();

      stringstream sss;
      sss << this_thread::get_id() << " cell: " << task.id << "-" << task.suffix
          << " polygons: " << task.geom.size() << " points: " << bg::num_points(task.geom)
          << " create poly: " << fixed << setprecision(7)
          << duration<double>(stage1 - start).count() << " sec."
          << " make cell : " << duration<double>(stage2 - stage1).count() << " sec.";
      LOG(LINFO, (sss.str()));
    }

    void SplitCellGeometry(Cell const & cell, vector<Cell> & outCells)
    {
      auto start = system_clock::now();
      SplitCell(cell, outCells);

      for (size_t i = 0; i < outCells.size(); ++i)
      {
        boost::geometry::intersection(outCells[i].bounds, cell.geom, outCells[i].geom);
        outCells[i].stage = (bg::num_points(outCells[i].geom) > kMaxPointsInCell)
                                ? Cell::EStage::NeedSplit
                                : Cell::EStage::Done;
      }
      auto end = system_clock::now();

      stringstream ss;
      ss << this_thread::get_id() << " Split cell: " << cell.id << "-" << cell.suffix
         << " polygons: " << cell.geom.size() << " points: " << bg::num_points(cell.geom);
      ss << " done in " << fixed << setprecision(7) << duration<double>(end - start).count()
         << " sec.";
      LOG(LINFO, (ss.str()));
    }

    void ProcessCell(Cell const & cell, vector<Cell> & outCells)
    {
      auto start = system_clock::now();
      try
      {
        switch (cell.stage)
        {
          case Cell::EStage::Init:
            MakeCellGeometry(cell, outCells);
            break;
          case Cell::EStage::NeedSplit:
            SplitCellGeometry(cell, outCells);
            break;
          case Cell::EStage::Done:
            break;
        }
      }
      catch (exception & e)
      {
        stringstream ss;
        ss << this_thread::get_id() << " cell: " << cell.id << "-" << cell.suffix
           << " failed: " << e.what();
        LOG(LERROR, (ss.str()));
        m_errorCell.push_back(cell);
        outCells.clear();
        for (auto p : cell.geom)
          CorrectAndCheckPolygon(p);
      }
      auto end = system_clock::now();

      // store ready result
      for (auto & cell : outCells)
      {
        if (cell.stage == Cell::EStage::Done && bg::num_points(cell.geom) != 0)
        {
          stringstream name;
          name << cell.id << cell.suffix;
          lock_guard<mutex> lock(m_mutexResult);
          m_processResultFunc(name.str(), cell.geom);
        }
      }
      stringstream ss;
      ss << this_thread::get_id() << " cell: " << cell.id << "-" << cell.suffix;
      ss << " processed in " << fixed << setprecision(7) << duration<double>(end - start).count()
         << " sec.";
      LOG(LINFO, (ss.str()));
    }

  public:
    void operator()()
    {
      // thread main loop
      for (;;)
      {
        Cell currentCell;
        unique_lock<mutex> lock(m_mutexTasks);
        m_listCondVar.wait(lock, [this]{return (!m_listTasks.empty() || m_inWork == 0);});
        if (m_listTasks.empty() && m_inWork == 0)
          break;

        currentCell = m_listTasks.front();
        m_listTasks.pop_front();
        ++m_inWork;
        lock.unlock();

        vector<Cell> resultCells;
        ProcessCell(currentCell, resultCells);

        lock.lock();
        // return to queue not ready cells
        for (auto const & cell : resultCells)
        {
          if (cell.stage == Cell::EStage::NeedSplit || cell.stage == Cell::EStage::Init)
            m_listTasks.push_back(cell);
        }
        --m_inWork;
        m_listCondVar.notify_all();
      }

      // return back cells with error into task queue
      if (!m_errorCell.empty())
      {
        unique_lock<mutex> lock(m_mutexTasks);
        m_listTasks.insert(m_listTasks.end(), m_errorCell.begin(), m_errorCell.end());
      }
    }
  };

}

bool CoastlineFeaturesGenerator::MakePolygons(list<FeatureBuilder1> & vecFb)
{
  bool allPolygonsValid = true;
  deque<polygon> polygons;
  for (auto const & region : m_regions)
  {
    polygons.push_back(polygon());
    for (auto const & p : region)
      polygons.back().outer().emplace_back(point{p.x, p.y});
    allPolygonsValid &= RegionMill::CorrectAndCheckPolygon(polygons.back());
  }

  if (!allPolygonsValid)
    return false;

  uint32_t numThreads = thread::hardware_concurrency();
  LOG(LINFO, ("Starting", numThreads, "threads."));
  return RegionMill::Process(numThreads, 5, polygons, [&vecFb, this](string const & name, multi_polygon & resultPolygon)
  {
    vecFb.emplace_back();
    vecFb.back().SetCoastCell(1 /* value used as bool */, name);

    for (auto const & polygonPart : resultPolygon)
    {
      vector<m2::PointD> ring;
      // outer ring
      ring.resize(polygonPart.outer().size());
      for (size_t i = 0; i < polygonPart.outer().size(); ++i)
        ring[i] = {polygonPart.outer()[i].x(), MercatorBounds::LatToY(polygonPart.outer()[i].y())};
      vecFb.back().AddPolygon(ring);
      // inner rings
      for (auto const & p : polygonPart.inners())
      {
        ring.resize(p.size());
        for (size_t i = 0; i < p.size(); ++i)
          ring[i] = {p[i].x(), MercatorBounds::LatToY(p[i].y())};
        vecFb.back().AddPolygon(ring);
      }
    }

    vecFb.back().SetArea();
    vecFb.back().AddType(m_coastType);
  });
}
