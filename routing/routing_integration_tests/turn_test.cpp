#include "testing/testing.hpp"

#include "routing/routing_integration_tests/routing_test_tools.hpp"

#include "generator/collector_city_area.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "routing/route.hpp"
#include "routing/routing_callbacks.hpp"


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
  for (size_t i = 0; i < points.size(); ++i)
  {
    auto const & prev = i == 0 ? points.back() : points[i - 1];
    auto const & cur = points[i];

    static m2::PointD const kZero = m2::PointD::Zero();
    auto const prevLen =
        convertToMeters ? MercatorBounds::DistanceOnEarth(kZero, prev) : prev.Length();
    auto const curLen =
        convertToMeters ? MercatorBounds::DistanceOnEarth(kZero, cur) : cur.Length();

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
  std::vector<m2::PointD> m_points;
};

struct Relation
{
  size_t m_id;
  double m_area = 0.0;
  Locality m_locality = Locality::Undef;
  size_t m_label = 0;
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
        if (input.good())
          input >> cached;
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
          output << all;
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
        } else if (key == "population")
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
        way.m_points.emplace_back(node.m_point);
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
    std::vector<m2::PointD> points;
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
          points.insert(points.end(), way.m_points.begin(), way.m_points.end());
        }
        else if (type == "node" && role == "label")
        {
          relation.m_label = ref;
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

    relation.m_area = FindPolygonArea(points, true /* convertToMeters */);

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

  Type GetType(string const & line, size_t & pos)
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
  {
    reader.ParseLine(*line);
  }

  std::cout << std::endl;

  reader.PrintTooBigRelations(Locality::Town);
  reader.DumpUrls(100);
}