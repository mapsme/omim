#include "testing/testing.hpp"

#include "routing/routing_integration_tests/routing_test_tools.hpp"

#include "generator/collector_city_area.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "routing/route.hpp"
#include "routing/routing_callbacks.hpp"

#include "geometry/convex_hull.hpp"
#include "geometry/polygon.hpp"

#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>
#include <chrono>
#include <random>

#include "boost/optional.hpp"

using namespace routing;
using namespace routing::turns;
using namespace std;

void AddPointWithRadius(ofstream & output, ms::LatLon const & latlon, double r)
{
  output << latlon.m_lat << " " << latlon.m_lon << " " << r << std::endl;
}

double FindPolygonArea(std::vector<m2::PointD> const & points, bool convertToMeters = true)
{
  if (points.empty())
    return 0.0;

  double result = 0.0;
  m2::PointD const & zero = points.front();
  for (size_t i = 0; i < points.size(); ++i)
  {
    auto const & prev = i == 0 ? points.back() : points[i - 1];
    auto const & cur = points[i];

//    static m2::PointD const kZero = m2::PointD::Zero();
    auto const prevLen =
        convertToMeters ? MercatorBounds::DistanceOnEarth(zero, prev) : prev.Length();
    auto const curLen =
        convertToMeters ? MercatorBounds::DistanceOnEarth(zero, cur) : cur.Length();

    if (base::AlmostEqualAbs(prevLen, 0.0, 1e-20) || base::AlmostEqualAbs(curLen, 0.0, 1e-20))
      continue;

    double sinAlpha = CrossProduct(prev, cur) / (prev.Length() * cur.Length());

    result += prevLen * curLen * sinAlpha / 2.0;
  }

  return std::abs(result);
}


enum class Locality
{
  City,
  Town,
  Village,
  Undef,
};

ftypes::LocalityType ConvertFromLocality(Locality locality)
{
  switch (locality)
  {
  case Locality::City: return ftypes::LocalityType::City;
  case Locality::Town: return ftypes::LocalityType::Town;
  case Locality::Village: return ftypes::LocalityType::Village;
  default: UNREACHABLE();
  }
}

Locality FromStringMisha(string const & str)
{
  if (str == "city")
    return Locality::City;
  if (str == "town")
    return Locality::Town;
  if (str == "village")
    return Locality::Village;

  return Locality::Undef;
}

struct Node
{
  ms::LatLon m_latlon;
  m2::PointD m_point;
  Locality m_locality = Locality::Undef;
  size_t m_population = 0;
  size_t m_id;
};

struct Way
{
  size_t m_id;
  std::vector<Node> m_points;
};

struct Relation
{
  size_t m_id;
  double m_area = 0.0;
  Locality m_locality = Locality::Undef;
  size_t m_label = 0;
  size_t m_admin_centre = 0;
};

class FileReaderMisha
{
public:

  enum class Type
  {
    Node,
    Way,
    Relation,
    Undef,
  };

  explicit FileReaderMisha(string const & path) : m_path(path), m_input(m_path), m_pointOfstream("/tmp/points")
  {
    ofstream point_fh("/tmp/cpp_points");
    CHECK(m_input.good(), (path));
  }

  boost::optional<string> GetNextLine()
  {
    static size_t all = 0;
    string line;
    if (!all)
    {
      size_t cached = 0;
      {
        ifstream input("/tmp/lines");
        string path(getenv("PATH"));
        string pathInInput;
        if (input.good())
        {
          input >> pathInInput >> cached;
          if (path != pathInInput)
            cached = 0;
        }
      }

      if (!cached)
      {
        ifstream input(m_path);
        size_t n = 0;
        while (getline(input, line))
        {
          if (n % 100000 == 0)
            std::cout << n << "\r";
          ++n;
        }
        all = n;
        {
          ofstream output("/tmp/lines");
          string path(getenv("PATH"));
          output << path << " " << all;
        }
      } else all = cached;
      std::cout << std::endl;
    }

    static size_t kReaded = 0;
    ++kReaded;
    if (kReaded % 300000 == 0)
    {
      double percent = static_cast<double>(kReaded) / all * 100;
      std::cout << kReaded << " / " << all << " (" << percent << "%)                      \r";
    }

    if (getline(m_input, line))
      return {std::move(line)};
    return {};
  }

  void ParseLine(string const & line)
  {
    size_t pos = 0;
    Type const type = GetType(line, pos);
    switch (type)
    {
    case Type::Node: return ParseNode(line);
    case Type::Way: return ParseWay(line);
    case Type::Relation: return ParseRelation(line);
    case Type::Undef: return;
    }
  }

  void JoinLanduse()
  {
    double const kLimitMeters = 300;
    auto const isClose = [kLimitMeters](auto const & area1, auto const & area2) {
      double minDist = std::numeric_limits<double>::max();
      for (auto const & point1 : area1) {
        for (size_t i = 0; i < area2.size() - 1; ++i)
        {
          m2::ParametrizedSegment<m2::PointD> segment(area2[i], area2[i + 1]);
          auto const closest = segment.ClosestPointTo(point1);
          minDist = std::min(minDist, MercatorBounds::DistanceOnEarth(point1, closest));
          if (minDist < kLimitMeters)
            return true;
        }
      }
      return false;
    };
    std::vector<std::vector<m2::PointD>> areasPrev;
    for (auto const & item : m_ways)
    {
      auto const & way = item.second;
      std::vector<m2::PointD> pointsToAdd;
      for (auto const & point : way.m_points)
        pointsToAdd.emplace_back(point.m_point);
      areasPrev.emplace_back(std::move(pointsToAdd));
    }

    for (;;)
    {
      std::vector<std::vector<m2::PointD>> areasCur;
      unordered_set<size_t> used;
      bool was = false;
      std::cout << "areasPrev.size() = " << areasPrev.size() << std::endl;
      for (size_t i = 0; i < areasPrev.size(); ++i)
      {
        if (used.count(i) != 0)
          continue;

        for (size_t j = i + 1; j < areasPrev.size(); ++j)
        {
          if (used.count(j) != 0)
            continue;

          if (isClose(areasPrev[i], areasPrev[j]))
          {
            was = true;
            areasPrev[i].insert(areasPrev[i].end(), areasPrev[j].begin(), areasPrev[j].end());
            used.emplace(j);
          }
        }

        areasCur.emplace_back(std::move(areasPrev[i]));
      }

      areasPrev = std::move(areasCur);
      areasCur.clear();
      for (auto const & polygon : areasPrev)
      {
        m2::ConvexHull convexHull(polygon, 1e-9);
        areasCur.emplace_back(convexHull.Points());
      }
      areasPrev = std::move(areasCur);

      if (!was)
        break;

    }

    ofstream output("/tmp/points");
    output << std::setprecision(20);
    std::cout << std::setprecision(20);
    for (auto const & polygon : areasPrev)
    {
      m2::ConvexHull convexHull(polygon, 1e-9);
      output << convexHull.Points().size() << std::endl;
      if (convexHull.Points().size() == 2)
      {
        std::cout << polygon.size() << std::endl;
        for (auto const & point : polygon)
          std::cout << "{" << point.x << "," << point.y << "}, ";
        std::cout << std::endl;
      }
      for (auto const & point : convexHull.Points())
      {
        auto const latlon = MercatorBounds::ToLatLon(point);
        output << latlon.m_lat << " " << latlon.m_lon << std::endl;
      }
    }
  }

  void PrintTooBigRelations(Locality type)
  {
    std::cout << "PrintTooBigRelations()" << std::endl;
    double all = 0;
    double part = 0;
    double circleLess = 0;
    double circleMore = 0;
    for (auto const & item : m_relations)
    {
      all += 1;
      size_t const id = item.first;

      auto const & relation = item.second;

      if (relation.m_label == 0)
        continue;

      if (relation.m_locality != Locality::Undef)
        continue;

      part += 1;

      auto const & node = m_nodes[relation.m_label];
      if (node.m_population == 0)
        continue;

      if (node.m_locality != type)
        continue;

      double r = ftypes::GetRadiusByPopulation(node.m_population);
      switch (node.m_locality)
      {
      case Locality::Town: r /= 2.0; break;
      case Locality::Village: r /= 3.0; break;
      default: r /= 1.0;
      }

      double const circleArea = M_PI * r*r;
      if (circleArea < relation.m_area)
      {
        circleLess += 1;
      }
      else
      {
        AddPointWithRadius(m_pointOfstream, node.m_latlon, r);
        AddUrl(id, Type::Relation);
        circleMore += 1;
      }
    }

    std::cout << "All: " << all << ", with label && without place=city|town|village: " << part
              << ", circleArea < relation.m_area:" << circleLess
              << ", circleArea > relation.m_area:" << circleMore << std::endl;
  }

  void ShowSmallestRealtionsWithAdminCentre(bool filterByArea = false, Locality filter = Locality::City)
  {
    unordered_map<size_t, std::vector<Relation>> nodeToRelations;
    for (auto const & item : m_relations)
    {
      auto const & relation = item.second;

      if (relation.m_admin_centre == 0)
        continue;

      auto const it = m_nodes.find(relation.m_admin_centre);
      if (it == m_nodes.cend())
        continue;
      auto const & node = it->second;
      Locality adminCentreType = node.m_locality;
      if (adminCentreType != filter)
        continue;

      nodeToRelations[node.m_id].emplace_back(relation);
    }

    size_t filtered = 0;
    size_t all = 0;
    size_t no_population = 0;
    for (auto const & item : nodeToRelations)
    {
      ++all;
      size_t minIndex = 0;
      double minArea = numeric_limits<double>::max();

      for (size_t i = 0; i < item.second.size(); ++i)
      {
        auto const & r = item.second[i];
        if (r.m_area > 1 && minArea > r.m_area)
        {
          minArea = r.m_area;
          minIndex = i;
        }
      }

      if (filterByArea)
      {
        auto const it = m_nodes.find(item.first);
        if (it == m_nodes.cend())
          continue;

        size_t const population = it->second.m_population;
        if (population == 0)
        {
          ++no_population;
          continue;
        }

        auto const locality = it->second.m_locality;
        double r = ftypes::GetRadiusByPopulationForRouting(population, ConvertFromLocality(locality));
        if (it->second.m_id == 2939672577)
        {
          PushDebugPoint(it->second.m_latlon, r);
          LOG(LINFO, ("r =", r, "pir^2 =", M_PI * r *r, "minArea =", minArea));
          auto mr = MercatorBounds::MetersToMercator(r);
          LOG(LINFO, ("mercator_pir^2 =", M_PI * mr * mr));
        }

        if (minArea > M_PI * r * r)
        {
          ++filtered;
          continue;
        }

//        PushDebugPoint(it->second.m_latlon, r);

        ofstream output("/tmp/population_to_area", ofstream::app);
        output << item.second[minIndex].m_id << " " << minArea << " " << population << endl;
      }

      AddUrl(item.second[minIndex].m_id, Type::Relation);
    }

    std::cout << "Relations with min area and admin_centre: " << all;
    if (filterByArea)
      std::cout << ", filtered by area: " << filtered << ", no_population: " << no_population;
    std::cout << std::endl;
  }

  void DumpUrls(size_t size)
  {
    static string kPath = "/tmp";
    auto const seed = std::chrono::system_clock::now().time_since_epoch().count();
    shuffle(m_urls.begin(), m_urls.end(), std::default_random_engine(static_cast<uint32_t>(seed)));
    size = min(size, m_urls.size());
    std::cout << "Have: " << m_urls.size() << " urls. Open: " << size << " urls" << std::endl;
    for (size_t i = 0; i < size; ++i)
    {
      ofstream output(kPath + "/" + std::to_string(i) + ".html");
      output << "<head><meta http-equiv=\"Refresh\" content=\"5; url=" << m_urls[i] << "\"></head>";
    }
  }

private:

  void PushDebugPoint(ms::LatLon const & latlon, double r = 0)
  {
    ofstream point_fh("/tmp/cpp_points", ofstream::app);
    point_fh << setprecision(20);
    point_fh << latlon.m_lat << " " << latlon.m_lon;
    if (r > 0)
      point_fh << " " << r;
    point_fh << std::endl;
  }

  size_t GetPopulation(Relation const & relation)
  {
    size_t nodeId = 0;
    if (relation.m_label)
      nodeId = relation.m_label;
    else if (relation.m_admin_centre)
      nodeId = relation.m_admin_centre;
    else
      return 0;

    auto const it = m_nodes.find(nodeId);
    if (it == m_nodes.cend())
      return 0;
    return it->second.m_population;
  }

  void AddUrl(size_t id, Type type)
  {
    string url = "https://www.openstreetmap.org/"; //relation/3959622";
    switch (type)
    {
    case Type::Node:
    {
      url += "node/";
      break;
    }
    case Type::Way:
    {
      url += "way/";
      break;
    }
    case Type::Relation:
    {
      url += "relation/";
      break;
    }
    default: CHECK(false, (id));
    }

    url += std::to_string(id);
    m_urls.emplace_back(std::move(url));
  }

  void ParseNode(string const & line)
  {
    static bool was = false;
    if (!was)
    {
      std::cout << "PARSE NODES                                                    " << std::endl;
      was = true;
    }

    size_t pos = 0;
    size_t id = 0;
    Parse(line, "id=", pos, id);

    double lat = 0.0;
    double lon = 0.0;
    Parse(line, "lat=", pos, lat);
    Parse(line, "lon=", pos, lon);

    ms::LatLon latlon(lat, lon);

    Node node;
    node.m_latlon = latlon;
    node.m_point = MercatorBounds::FromLatLon(latlon);
    node.m_id = id;

    if (GetPos(line, "/>", pos, false /* mustFind */))
    {
      m_nodes[id] = node;
      return;
    }

    boost::optional<string> newLineOp;
    while ((newLineOp = GetNextLine()))
    {
      auto const & newLine = *newLineOp;
      if (IsSubstr(newLine, "</node>"))
        break;

      if (IsSubstr(newLine, "<tag"))
      {
        string key;
        string value;
        pos = 0;
        Parse(newLine, "k=", pos, key);

        if (key == "place")
        {
          Parse(newLine, "v=", pos, value);
          node.m_locality = FromStringMisha(value);
        }
        else if (key == "population")
        {
          Parse(newLine, "v=", pos, value);
          size_t const population = generator::ParsePopulationSting(value);
          node.m_population = population;
        }
      }
    }

    m_nodes[id] = node;
  }

  void ParseWay(string const & line)
  {
    static bool was = false;
    if (!was)
    {
      std::cout << "PARSE WAYS                                                    " << std::endl;
      was = true;
    }

    Way way;
    size_t pos = 0;
    size_t id = 0;
    Parse(line, "id=", pos, id);
    way.m_id = id;

    boost::optional<string> newLine;
    while ((newLine = GetNextLine()))
    {
      if (IsSubstr(*newLine, "</way>"))
        break;

      if (IsSubstr(*newLine, "<nd"))
      {
        size_t ref;
        size_t tmp = 0;
        Parse(*newLine, "ref=", tmp, ref);
        auto const it = m_nodes.find(ref);
        if (it == m_nodes.cend())
          continue;

        auto const & node = it->second;
        way.m_points.emplace_back(node);
      }
    }

    m_ways.emplace(id, std::move(way));
  }

  void ParseRelation(string const & line)
  {
    static bool was = false;
    if (!was)
    {
      std::cout << "PARSE RELATIONS                                                    " << std::endl;
      was = true;
    }

    Relation relation;
    size_t pos = 0;
    size_t id;
    Parse(line, "id=", pos, id);
    relation.m_id = id;

    bool add = false;

    boost::optional<string> newLine;
    std::vector<std::vector<m2::PointD>> points;
    std::unordered_set<uint64_t> usedNodeIds;
    while ((newLine = GetNextLine()))
    {
      if (IsSubstr(*newLine, "</relation>"))
        break;

      if (IsSubstr(*newLine, "<member"))
      {
        string type;
        pos = 0;
        Parse(*newLine, "type=", pos, type);
        size_t ref;
        Parse(*newLine, "ref=", pos, ref);
        string role;
        Parse(*newLine, "role=", pos, role);

        if (role == "outer")
        {
          auto const it = m_ways.find(ref);
          if (it == m_ways.cend())
            continue;

          auto const & way = it->second;
          bool hasIntersection = false;
          for (auto const & point : way.m_points)
          {
            if (usedNodeIds.count(point.m_id))
            {
              hasIntersection = true;
              break;
            }
          }

          if (!hasIntersection)
          {
            usedNodeIds.clear();
            points.emplace_back();
          }

          for (auto const & point : way.m_points)
          {
            points.back().emplace_back(point.m_point);
            usedNodeIds.emplace(point.m_id);
          }
        }
        else if (type == "node" && role == "label")
        {
          relation.m_label = ref;
        }
        else if (type == "node" && role == "admin_centre")
        {
          relation.m_admin_centre = ref;
        }
      }
      else if (IsSubstr(*newLine, "<tag"))
      {
        string key;
        string value;
        pos = 0;
        Parse(*newLine, "k=", pos, key);

        if (key == "place")
        {
          Parse(*newLine, "v=", pos, value);
          relation.m_locality = FromStringMisha(value);
        }
        else if (key == "boundary")
        {
          Parse(*newLine, "v=", pos, value);
          if (value == "administrative")
            add = true;
        }
      }
    }

    size_t i = 0;
    size_t best = 0;
    for (auto const & polygon : points)
    {
      auto a = GetPolygonArea(polygon.begin(), polygon.end());
      a = MercatorBounds::MercatorSqrToMetersSqr(a);
      if (relation.m_area < a)
      {
        relation.m_area = a;
        best = i;
      }
      ++i;
    }

    if (id == 9656805)
    {
      auto const & polygon = points[best];
      LOG(LINFO, ("GetPolygonArea(polygon.begin(), polygon.end()) =", GetPolygonArea(polygon.begin(), polygon.end())));
      LOG(LINFO, ("FindPolygonArea(polygon) =", FindPolygonArea(polygon, false), "m^2 =", FindPolygonArea(polygon)));
      for (auto const & point : polygon)
        PushDebugPoint(MercatorBounds::ToLatLon(point), 0.0000001);

      m2::ConvexHull convexHull(polygon, 1e-9);
      double area = 0;
      auto const & base = polygon.front();
      for (size_t i = 1; i < polygon.size() - 1; ++i)
        area += MercatorBounds::AreaOnEarth(base, polygon[i], polygon[i + 1]);
      LOG(LINFO, ("AreaOnEarth =", area));
    }

    if (add)
      m_relations.emplace(id, relation);
  }

  static bool IsSubstr(string const & line, string const & substr)
  {
    return line.find(substr) != string::npos;
  }

  template <typename T>
  void Parse(string const & line, string const & subline, size_t & pos, T & result)
  {
    GetPos(line, subline, pos);
    size_t start = pos + subline.size() + 1;
    pos = start;
    while (line[pos] != '"')
      ++pos;
    string sub(line.begin() + start, line.begin() + pos);
    stringstream ss;
    ss << sub;
    ss >> result;
  }

  static bool GetPos(string const & line, string const & subline, size_t & pos, bool mustFind = true)
  {
    for (size_t i = pos; i < line.size(); ++i)
    {
      bool good = true;
      for (size_t j = 0; j < subline.size(); ++j)
      {
        if (line[i + j] != subline[j])
        {
          good = false;
          break;
        }
      }

      if (good)
      {
        pos = i;
        return true;
      }
    }

    if (mustFind)
      CHECK(false, (line, subline, pos));

    return false;
  }

  static Type GetType(string const & line, size_t & pos)
  {
    if (IsSubstr(line, "<node"))
      return Type::Node;
    if (IsSubstr(line, "<way"))
      return Type::Way;
    if (IsSubstr(line, "<relation"))
      return Type::Relation;

    return Type::Undef;
  }

  string m_path;
  ifstream m_input;
  unordered_map<size_t, Node> m_nodes;
  unordered_map<size_t, Way> m_ways;
  unordered_map<size_t, Relation> m_relations;
  vector<string> m_urls;
  ofstream m_pointOfstream;
};

UNIT_TEST(Toolsa)
{
  CHECK(std::getenv("PATH") && !std::string(std::getenv("PATH")).empty(), ());
  std::cout << std::unitbuf;
  std::string path = std::string(std::getenv("PATH"));

  FileReaderMisha reader(path);
  boost::optional<string> line;
  while ((line = reader.GetNextLine()))
    reader.ParseLine(*line);

  std::cout << std::endl;

  reader.ShowSmallestRealtionsWithAdminCentre(true /* filterByArea */, Locality::Town);
//  reader.JoinLanduse();
//  reader.PrintTooBigRelations(Locality::Town);
  reader.DumpUrls(100);
}