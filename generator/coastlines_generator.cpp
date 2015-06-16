#include "generator/coastlines_generator.hpp"
#include "generator/feature_builder.hpp"

#include "indexer/point_to_int64.hpp"

#include "geometry/region2d/binary_operators.hpp"

#include "base/string_utils.hpp"
#include "base/logging.hpp"

#include "std/bind.hpp"
#include "std/condition_variable.hpp"
#include "std/function.hpp"
#include "std/thread.hpp"
#include "std/utility.hpp"
#include "std/fstream.hpp"
#include "std/functional.hpp"

#include "boost/geometry.hpp"
#include "boost/geometry/geometries/point_xy.hpp"
#include "boost/geometry/algorithms/detail/overlay/turn_info.hpp"
#include "boost/geometry/extensions/algorithms/dissolve.hpp"

typedef m2::RegionI RegionT;
typedef m2::PointI PointT;
typedef m2::RectI RectT;

CoastlineFeaturesGenerator::CoastlineFeaturesGenerator(uint32_t coastType)
  : m_merger(POINT_COORD_BITS), m_coastType(coastType)
{
  string dumpName = (intermediateDir + "/merged_coastline_double.dump");
  LOG(LINFO, ("Dump double coastsline into:", dumpName));
  m_dumpStream.open(dumpName);
}

namespace
{
  m2::RectD GetLimitRect(RegionT const & rgn)
  {
    RectT r = rgn.GetRect();
    return m2::RectD(r.minX(), r.minY(), r.maxX(), r.maxY());
  }

  inline PointT D2I(m2::PointD const & p)
  {
    m2::PointU const pu = PointD2PointU(p, POINT_COORD_BITS);
    return PointT(static_cast<int32_t>(pu.x), static_cast<int32_t>(pu.y));
  }

  template <class TreeT> class DoCreateRegion
  {
    TreeT & m_tree;

    RegionT m_rgn;
    m2::PointD m_pt;
    bool m_exist;

  public:
    DoCreateRegion(TreeT & tree) : m_tree(tree), m_exist(false) {}

    bool operator()(m2::PointD const & p)
    {
      // This logic is to skip last polygon point (that is equal to first).

      if (m_exist)
      {
        // add previous point to region
        m_rgn.AddPoint(D2I(m_pt));
      }
      else
        m_exist = true;

      // save current point
      m_pt = p;
      return true;
    }

    void EndRegion()
    {
      m_tree.Add(m_rgn, GetLimitRect(m_rgn));

      m_rgn = RegionT();
      m_exist = false;
    }
  };


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

void CoastlineFeaturesGenerator::AddRegionToTree(FeatureBuilder1 const & fb)
{
  ASSERT ( fb.IsGeometryClosed(), () );

  DoCreateRegion<TTree> createRgn(m_tree);
  fb.ForEachGeometryPointEx(createRgn);
  DoCreateDoubleRegion region;
  fb.ForEachGeometryPointEx(region);
  uint32_t sz = static_cast<uint32_t>(region.m_rgn.size());
  m_dumpStream.write((char *)&sz, sizeof(uint32_t));
  m_dumpStream.write((char *)region.m_rgn.data(), region.m_rgn.size() * sizeof(m2::PointD));
  m_regions.push_back(region.m_rgn);
}

void CoastlineFeaturesGenerator::operator()(FeatureBuilder1 const & fb)
{
  if (fb.IsGeometryClosed())
    AddRegionToTree(fb);
  else
    m_merger(fb);
}

namespace
{
  class DoAddToTree : public FeatureEmitterIFace
  {
    CoastlineFeaturesGenerator & m_rMain;
    size_t m_notMergedCoastsCount;
    size_t m_totalNotMergedCoastsPoints;

  public:
    DoAddToTree(CoastlineFeaturesGenerator & rMain)
      : m_rMain(rMain), m_notMergedCoastsCount(0), m_totalNotMergedCoastsPoints(0) {}

    virtual void operator() (FeatureBuilder1 const & fb)
    {
      if (fb.IsGeometryClosed())
        m_rMain.AddRegionToTree(fb);
      else
      {
        LOG(LINFO, ("Not merged coastline", fb.GetOsmIdsString()));
        ++m_notMergedCoastsCount;
        m_totalNotMergedCoastsPoints += fb.GetPointsCount();
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

    size_t GetNotMergedCoastsPoints() const
    {
      return m_totalNotMergedCoastsPoints;
    }
  };
}

bool CoastlineFeaturesGenerator::Finish(bool needStopIfFail, string const & intermediateDir)
{
  DoAddToTree doAdd(*this);
  m_merger.DoMerge(doAdd);

  uint32_t term = 0;
  m_dumpStream.write((char *)&term, sizeof(uint32_t));
  m_dumpStream.flush();
  m_dumpStream.close();

  if (doAdd.HasNotMergedCoasts())
  {
    LOG(LINFO, ("Total not merged coasts:", doAdd.GetNotMergedCoastsCount()));
    LOG(LINFO, ("Total points in not merged coasts:", doAdd.GetNotMergedCoastsPoints()));
    return false;
  }

  return true;
}

namespace
{
class DoDifference
{
  RectT m_src;
  vector<RegionT> m_res;
  vector<m2::PointD> m_points;

public:
  DoDifference(RegionT const & rgn)
  {
    m_res.push_back(rgn);
    m_src = rgn.GetRect();
  }

  void operator()(RegionT const & r)
  {
    // if r is fully inside source rect region,
    // put it to the result vector without any intersection
    if (m_src.IsRectInside(r.GetRect()))
      m_res.push_back(r);
    else
      m2::IntersectRegions(m_res.front(), r, m_res);
  }

  void operator()(PointT const & p)
  {
    m_points.push_back(PointU2PointD(
        m2::PointU(static_cast<uint32_t>(p.x), static_cast<uint32_t>(p.y)), POINT_COORD_BITS));
  }

  size_t GetPointsCount() const
  {
    size_t count = 0;
    for (size_t i = 0; i < m_res.size(); ++i)
      count += m_res[i].GetPointsCount();
    return count;
  }

    void AssignGeometry(FeatureBuilder1 & fb)
    {
      for (size_t i = 0; i < m_res.size(); ++i)
      {
        m_points.clear();
        m_points.reserve(m_res[i].Size() + 1);

        m_res[i].ForEachPoint(ref(*this));

        fb.AddPolygon(m_points);
      }
    }
  };

  namespace bg = boost::geometry;
  namespace bgi = boost::geometry::index;

  typedef bg::model::d2::point_xy<double> point;
  typedef bg::model::box<point> box;
  typedef bg::model::polygon<point> polygon;
  typedef std::pair<box, size_t> value;

  typedef boost::geometry::model::multi_polygon<polygon, std::deque> multi_polygon;


  std::ostream & operator << (std::ostream & s, multi_polygon const & multipolygon)
  {
    size_t ringNum = 1;
    s << "multipolygon_" << multipolygon.size() << std::endl;
    for (auto const & p : multipolygon)
    {
      s << ringNum++ << std::endl;
      for(auto const & op : p.outer())
        s << std::fixed << std::setprecision(7) << op.get<0>() << " " << my::RadToDeg(2.0 * atan(tanh(0.5 * my::DegToRad(op.get<1>())))) << std::endl;
      s << "END" << std::endl;
      for(auto const & ring : p.inners())
      {
        s << "!" << ringNum++ << (ring.size() < 3 ? "invalid" : "") << std::endl;
        for(auto const & op : ring)
          s << std::fixed << std::setprecision(7) << op.get<0>() << " " << my::RadToDeg(2.0 * atan(tanh(0.5 * my::DegToRad(op.get<1>())))) << std::endl;
        s << "END" << std::endl;
      }
    }
    s << "END" << std::endl;
    return s;
  }


  class RegionMill
  {
    typedef bgi::rtree< value, bgi::quadratic<16>> TIndex;
    typedef std::deque<polygon> TContainer;

    TContainer const & m_container;
    TIndex const & m_index;

    typedef function<void(string const &, multi_polygon &)> TProcessResultFunc;

    TProcessResultFunc m_processResultFunc;

    struct Task
    {
      size_t cellId;
      std::string subCell;
      box cellBounds;
      multi_polygon poly;
    };

    std::mutex & m_mutexTasks;
    std::list<Task> & m_listTasks;
    std::mutex & m_mutexResult;

    enum { kMaxPointsInCell = 20000 };

    typedef std::pair<bool, Task> TActualTask;

  public:
    static void Process(uint32_t numThreads, uint32_t baseScale, std::deque<polygon> const & polygons, TProcessResultFunc funcResult)
    {
      TIndex rtree;

      for (size_t i = 0; i < polygons.size(); ++i)
        rtree.insert(value(bg::return_envelope<box>(polygons[i]), i));

      std::list<RegionMill::Task> listTasks = RegionMill::CreateCellsForScale(baseScale);
      std::list<multi_polygon> listResult;

      std::vector<RegionMill> uniters;
      std::vector<std::thread> threads;
      std::mutex mutexTasks;
      std::mutex mutexResult;
      for (uint32_t i = 0; i < numThreads; ++i)
      {
        uniters.emplace_back(RegionMill(listTasks, mutexTasks, funcResult, mutexResult, polygons, rtree));
        threads.emplace_back(thread(uniters.back()));
      }

      for (auto & thread : threads)
        thread.join();
    }

  protected:

    RegionMill(std::list<Task> & listTasks, std::mutex & mutexTasks,
               TProcessResultFunc funcResult, std::mutex & mutexResult,
               TContainer const &container, TIndex const & index)
    : m_container(container)
    , m_index(index)
    , m_processResultFunc(funcResult)
    , m_mutexTasks(mutexTasks)
    , m_listTasks(listTasks)
    , m_mutexResult(mutexResult)
    {}

    static std::list<Task> CreateCellsForScale(uint32_t scale, uint32_t from = 0, uint32_t num = 0)
    {
      uint32_t numCells = 1 << scale;
      uint32_t cellSize = ((1 << POINT_COORD_BITS) - 1) / numCells;

      std::list<Task> result;

      uint32_t fromCell = from;
      uint32_t toCell = (num == 0) ? (numCells * numCells) : fromCell + num;

      for (uint32_t i = fromCell; i < toCell; ++i)
      {
        uint32_t xCell = i % numCells;
        uint32_t yCell = i / numCells;
        m2::PointD minPoint = PointU2PointD(m2::PointU((xCell * cellSize), (yCell * cellSize)), POINT_COORD_BITS);
        m2::PointD maxPoint = PointU2PointD(m2::PointU((xCell * cellSize + cellSize), (yCell * cellSize + cellSize)), POINT_COORD_BITS);

        box cell(point(minPoint.x, minPoint.y) , point(maxPoint.x, maxPoint.y));
        result.push_back({i, "", cell, {}});
      }
      return result;
    }

    TActualTask GetTask()
    {
      TActualTask task{false, Task()};

      std::lock_guard<std::mutex> lock(m_mutexTasks);
      if (m_listTasks.empty())
        return task;
      task = std::make_pair(true, m_listTasks.front());
      m_listTasks.pop_front();
      return std::move(task);
    }

    void PutTask(Task & task)
    {
      std::lock_guard<std::mutex> lock(m_mutexTasks);
      m_listTasks.push_back(task);
    }


    bool MakeCellGeometry(Task & task)
    {
      multi_polygon poly;

      // fetch regions from R-Tree by cell bounds
      for (auto it = m_index.qbegin(bgi::intersects(task.cellBounds)); it != m_index.qend(); ++it)
        poly.push_back(m_container[it->second]);

      // clip regions by cell bounds and invert it
      boost::geometry::intersection(task.cellBounds, poly, task.poly);
      poly.swap(task.poly);
      bg::clear(task.poly);
      boost::geometry::difference(task.cellBounds, poly, task.poly);

      if (bg::num_points(task.poly) > kMaxPointsInCell) {
        PutTask(task);
        return false;
      }
      return true;
    }

    bool SplitCellGeometry(Task & task)
    {
      if (bg::num_points(task.poly) <= kMaxPointsInCell)
        return true;

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
      double middleX = (task.cellBounds.min_corner().x() + task.cellBounds.max_corner().x()) / 2;
      double middleY = (task.cellBounds.min_corner().y() + task.cellBounds.max_corner().y()) / 2;

      std::array<box, 4> quarters;
      quarters[0] = task.cellBounds;
      quarters[0].max_corner() = point(middleX, middleY);
      quarters[1] = task.cellBounds;
      quarters[1].min_corner().x(middleX);
      quarters[1].max_corner().y(middleY);
      quarters[2] = task.cellBounds;
      quarters[2].min_corner().y(middleY);
      quarters[2].max_corner().x(middleX);
      quarters[3] = task.cellBounds;
      quarters[3].min_corner() = point(middleX, middleY);

      for (size_t i = 0; i < quarters.size(); ++i)
      {
        Task newTask;
        newTask.cellId = task.cellId;
        newTask.cellBounds = quarters[i];
        newTask.subCell = task.subCell + char('0'+i);
        boost::geometry::intersection(newTask.cellBounds, task.poly, newTask.poly);
        PutTask(newTask);
      }
      return false;
    }
public:
    void operator()()
    {
      for (;;)
      {
        TActualTask task = GetTask();
        if (!task.first)
          break;

        bool done = false;
        auto start = std::chrono::system_clock::now();
        try
        {
          done = (task.second.poly.empty()) ? MakeCellGeometry(task.second) : SplitCellGeometry(task.second);
        }
        catch (exception & e)
        {
          std::lock_guard<std::mutex> lock(m_mutexResult);
          stringstream ss;
          ss << "!!!!!!!!" << this_thread::get_id() << " cell: " << task.second.cellId << "-" << task.second.subCell << " failed: " << e.what();
          LOG(LERROR, (ss.str()));
        }
        auto end = std::chrono::system_clock::now();

        if (done)
        {
          std::lock_guard<std::mutex> lock(m_mutexResult);
          stringstream ss;
          ss << this_thread::get_id() << " cell: " << task.second.cellId << "-" << task.second.subCell << " polygons out: " << task.second.poly.size()
          << " points: " << bg::num_points(task.second.poly) << " done in " << std::chrono::duration<double>(end - start).count() << " sec.";
          LOG(LINFO, (ss.str()));
          if (!task.second.poly.empty())
          {
            stringstream name;
            name << task.second.cellId << task.second.subCell;
            m_processResultFunc(name.str(), task.second.poly);
            std::stringstream fname;
            fname << "./cell" << std::setfill('0') << std::setw(8) << task.second.cellId << task.second.subCell << ".poly";
            ofstream file(fname.str().c_str());
            file << task.second.poly << std::endl;
          }
        }

      }
    }
  };

  void CorrectAndCheckPolygon(polygon & p)
  {
    // correct orientation (ccw, cw), remove duplicate points
    bg::correct(p);

    bg::validity_failure_type failureType;

    if (bg::is_valid(p, failureType))
      return;

    std::ostringstream message;
    message << std::fixed << std::setprecision(7);
    bg::failing_reason_policy<> policy_visitor(message);
    is_valid(p, policy_visitor);
    LOG(LINFO, ("Num points:", bg::num_points(p), "Error:", message.str()));

    // correct self intersect geometry
    if (failureType == bg::failure_self_intersections)
    {
      multi_polygon out;
      bg::dissolve(p, out);
      LOG(LINFO, ("After dissolve:", bg::num_points(out), "polygons:", bg::num_geometries(out)));
      sort(out.begin(), out.end(), [](polygon const & p1, polygon const & p2) {return bg::num_points(p2) < bg::num_points(p1);});
      bg::assign(p, out.front());
    }
    if (failureType == bg::failure_spikes)
    {
      bg::remove_spikes(p);
      LOG(LINFO, ("After remove spikes:", bg::num_points(p), "polygons:", bg::num_geometries(p)));
    }
    LOG(LINFO, ("Fixed points:", bg::num_points(p), "polygons:", bg::num_geometries(p)));
  }
}

size_t CoastlineFeaturesGenerator::DumpCoastlines(string const & intermediateDir) const
{
  string dumpName = (intermediateDir + "/merged_coastline.dump");
  LOG(LINFO, ("Dump coastsline into:", dumpName));
  ofstream f(dumpName);
  size_t regionsNum = 0;
  GetRegionTree().ForEach([&regionsNum, &f](m2::RegionI const &region)
  {
    uint32_t sz = static_cast<uint32_t>(region.Data().size());
    f.write((char *)&sz, sizeof(uint32_t));
    f.write((char *)region.Data().data(), region.Data().size() * sizeof(m2::RegionI::ValueT));
    regionsNum++;
  });

  uint32_t term = 0;
  f.write((char *)&term, sizeof(uint32_t));
  f.flush();
  f.close();
  return regionsNum;
}


bool CoastlineFeaturesGenerator::GetFeature(CellIdT const & cell, FeatureBuilder1 & fb)
{
  // get rect cell
  double minX, minY, maxX, maxY;
  CellIdConverter<MercatorBounds, CellIdT>::GetCellBounds(cell, minX, minY, maxX, maxY);

    // create rect region
    PointT arr[] = {D2I(m2::PointD(minX, minY)), D2I(m2::PointD(minX, maxY)),
                    D2I(m2::PointD(maxX, maxY)), D2I(m2::PointD(maxX, minY))};
    RegionT rectR(arr, arr + ARRAY_SIZE(arr));

    // Do 'and' with all regions and accumulate the result, including bound region.
    // In 'odd' parts we will have an ocean.
    DoDifference doDiff(rectR);
    m_index.ForEachInRect(GetLimitRect(rectR), bind<void>(ref(doDiff), _1));

    // Check if too many points for feature.
    if (cell.Level() < kHighLevel && doDiff.GetPointsCount() >= kMaxPoints)
      return false;

    m_ctx.processResultFunc(cell, doDiff);
    return true;
  }

  void operator()()
  {
    // thread main loop
    while (true)
    {
      unique_lock<mutex> lock(m_ctx.mutexTasks);
      m_ctx.listCondVar.wait(lock, [this]{return (!m_ctx.listTasks.empty() || m_ctx.inWork == 0);});
      if (m_ctx.listTasks.empty())
        break;

  deque<polygon> polygons;
  for (auto const &region : m_regions)
  {
    polygons.push_back(polygon());
    for (auto const & p : region)
    {
      polygons.back().outer().emplace_back(point{p.x, p.y});
    }
    CorrectAndCheckPolygon(polygons.back());
  }

  uint32_t numThreads = thread::hardware_concurrency();
  LOG(LINFO, ("Starting", numThreads, "threads."));
  RegionMill::Process(numThreads, 5, polygons, [&vecFb, this](string const & name, multi_polygon & resultPolygon)
  {
    vecFb.emplace_back();
    vecFb.back().SetCoastCell(1 /* value used as bool */, name);

    for (auto const & polygonPart : resultPolygon)
    {
      vector<m2::PointD> ring;
      // outer ring
      ring.resize(polygonPart.outer().size());
      for (size_t i=0; i < polygonPart.outer().size(); ++i)
        ring[i] = {polygonPart.outer()[i].x(), MercatorBounds::LatToY(polygonPart.outer()[i].y())};
      vecFb.back().AddPolygon(ring);
      // inner rings
      for (auto const & p : polygonPart.inners())
      {
        ring.resize(p.size());
        for (size_t i=0; i < p.size(); ++i)
          ring[i] = {p[i].x(), MercatorBounds::LatToY(p[i].y())};
        vecFb.back().AddPolygon(ring);
      }
    }

    vecFb.back().SetArea();
    vecFb.back().AddType(m_coastType);
  });
}
