#include "testing/testing.hpp"

#include "coding/traffic.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"

#include "base/logging.hpp"
#include "base/math.hpp"

namespace coding
{
double CalculateLength(vector<TrafficGPSEncoder::DataPoint> const & path)
{
  double res = 0;
  for (size_t i = 1; i < path.size(); ++i)
  {
    auto p1 = MercatorBounds::FromLatLon(path[i - 1].m_latLon.lat, path[i - 1].m_latLon.lon);
    auto p2 = MercatorBounds::FromLatLon(path[i].m_latLon.lat, path[i].m_latLon.lon);
    res += MercatorBounds::DistanceOnEarth(p1, p2);
  }
  return res;
}

void Test(vector<TrafficGPSEncoder::DataPoint> & points)
{
  double const kEps = 1e-5;

  for (uint32_t version = 0; version <= TrafficGPSEncoder::kLatestVersion; ++version)
  {
    vector<uint8_t> buf;
    MemWriter<decltype(buf)> memWriter(buf);
    UNUSED_VALUE(TrafficGPSEncoder::SerializeDataPoints(version, memWriter, points));

    vector<TrafficGPSEncoder::DataPoint> result;
    MemReader memReader(buf.data(), buf.size());
    ReaderSource<MemReader> src(memReader);
    TrafficGPSEncoder::DeserializeDataPoints(version, src, result);

    TEST_EQUAL(points.size(), result.size(), ());
    for (size_t i = 0; i < points.size(); ++i)
    {
      TEST_EQUAL(points[i].m_timestamp, result[i].m_timestamp,
                 (points[i].m_timestamp, result[i].m_timestamp));
      TEST(my::AlmostEqualAbsOrRel(points[i].m_latLon.lat, result[i].m_latLon.lat, kEps),
           (points[i].m_latLon.lat, result[i].m_latLon.lat));
      TEST(my::AlmostEqualAbsOrRel(points[i].m_latLon.lon, result[i].m_latLon.lon, kEps),
           (points[i].m_latLon.lon, result[i].m_latLon.lon));
    }

    if (version == TrafficGPSEncoder::kLatestVersion)
    {
      LOG(LINFO, ("path length =", CalculateLength(points), "num points =", points.size(),
                  "compressed size =", buf.size()));
    }
  }
}

UNIT_TEST(Traffic_Serialization_Smoke)
{
  vector<TrafficGPSEncoder::DataPoint> data = {
      {0, ms::LatLon(0.0, 1.0)}, {0, ms::LatLon(0.0, 2.0)},
  };
  Test(data);
}

UNIT_TEST(Traffic_Serialization_EmptyPath)
{
  vector<TrafficGPSEncoder::DataPoint> data;
  Test(data);
}

UNIT_TEST(Traffic_Serialization_StraightLine100m)
{
  vector<TrafficGPSEncoder::DataPoint> path = {
      {0, ms::LatLon(0.0, 0.0)}, {0, ms::LatLon(0.0, 1e-3)},
  };
  Test(path);
}

UNIT_TEST(Traffic_Serialization_StraightLine50Km)
{
  vector<TrafficGPSEncoder::DataPoint> path = {
      {0, ms::LatLon(0.0, 0.0)}, {0, ms::LatLon(0.0, 0.5)},
  };
  Test(path);
}

UNIT_TEST(Traffic_Serialization_Zigzag500m)
{
  vector<TrafficGPSEncoder::DataPoint> path;
  for (size_t i = 0; i < 5; ++i)
  {
    double const x = i * 1e-3;
    double const y = i % 2 == 0 ? 0 : 1e-3;
    path.emplace_back(TrafficGPSEncoder::DataPoint(0, ms::LatLon(y, x)));
  }
  Test(path);
}

UNIT_TEST(Traffic_Serialization_Zigzag10Km)
{
  vector<TrafficGPSEncoder::DataPoint> path;
  for (size_t i = 0; i < 10; ++i)
  {
    double const x = i * 1e-2;
    double const y = i % 2 == 0 ? 0 : 1e-2;
    path.emplace_back(TrafficGPSEncoder::DataPoint(0, ms::LatLon(y, x)));
  }
  Test(path);
}

UNIT_TEST(Traffic_Serialization_Zigzag100Km)
{
  vector<TrafficGPSEncoder::DataPoint> path;
  for (size_t i = 0; i < 1000; ++i)
  {
    double const x = i * 1e-1;
    double const y = i % 2 == 0 ? 0 : 1e-1;
    path.emplace_back(TrafficGPSEncoder::DataPoint(0, ms::LatLon(y, x)));
  }
  Test(path);
}

UNIT_TEST(Traffic_Serialization_Circle20KmRadius)
{
  vector<TrafficGPSEncoder::DataPoint> path;
  size_t const n = 100;
  for (size_t i = 0; i < n; ++i)
  {
    double const alpha = 2 * math::pi * i / n;
    double const radius = 0.25;
    double const x = radius * cos(alpha);
    double const y = radius * sin(alpha);
    path.emplace_back(TrafficGPSEncoder::DataPoint(0, ms::LatLon(y, x)));
  }
  Test(path);
}

UNIT_TEST(Traffic_Serialization_ExtremeLatLon)
{
  vector<TrafficGPSEncoder::DataPoint> path = {
      {0, ms::LatLon(-90, -180)}, {0, ms::LatLon(90, 180)},
  };
  Test(path);
}
}  // namespace coding
