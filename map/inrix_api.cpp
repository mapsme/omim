#include "map/inrix_api.hpp"

#include "geometry/latlon.hpp"
#include "geometry/mercator.hpp"

#include "coding/url_encode.hpp"

#include "platform/platform.hpp"

#include "base/gmtime.hpp"
#include "base/string_utils.hpp"
#include "base/logging.hpp"
#include "base/math.hpp"

#include "std/algorithm.hpp"
#include "std/iostream.hpp"
#include "std/sstream.hpp"

#include "std/fstream.hpp"

#include "3party/pugixml/src/pugixml.hpp"

#include "private.h"

namespace
{

//#define INRIX_PACK_SEGMENTS
//#ifdef INRIX_PARSE_ORIGINAL

string const kGetSecurityToken = "http://api.inrix.com/Traffic/Inrix.ashx?" \
                                 "Action=GetSecurityToken&VendorID=" \
                                 INRIX_VENDOR_ID \
                                 "&ConsumerID=" \
                                 INRIX_CONSUMER_ID;

vector<string> const kRegionIds = { "EU", "NA" };

#ifdef INRIX_PARSE_ORIGINAL

double ConvertFirstCoordinate(long long val)
{
  double const s = (val > 0) ? 1 : ((val < 0) ? -1 : 0);
  return (((double)val - 0.5 * s) * 360.0) / (1 << 24);
}

double ConvertLastCoordinate(double degVal, long long val)
{
  return degVal + (double)val / 100000.0;
}

#endif

df::TrafficSpeedBucket GetSpeedBucket(int speed, int referenceSpeed)
{
  double const k = static_cast<double>(speed) / referenceSpeed;
  if (k < 0.4)
    return df::TrafficSpeedBucket::VerySlow;
  if (k < 0.8)
    return df::TrafficSpeedBucket::Slow;

  return df::TrafficSpeedBucket::Normal;
}

} // namespace

InrixApi::InrixApi()
  : m_regionId(kRegionIds[0])
{
}

void InrixApi::Authenticate()
{
  auto const callback = [this](downloader::HttpRequest & answer)
  {
    lock_guard<mutex> lock(m_mutex);
    string const & responseStr = answer.Data();
    pugi::xml_document document;
    if (!document.load_buffer(responseStr.c_str(), responseStr.length()))
    {
      LOG(LWARNING, ("Cannot parse server response:", responseStr));
      return;
    }

    auto authTokenNode = document.select_node("Inrix/AuthResponse/AuthToken").node();
    m_authToken = authTokenNode.text().as_string("");

    string const kTimestampFormat = "%FT%TZ";
    string const ts = document.select_node("Inrix").node().attribute("createdDate").as_string("");
    system_clock::time_point const serverTime = strings::ToTimePoint(ts, kTimestampFormat);
    string const expiry = authTokenNode.attribute("expiry").as_string("");
    m_expirationTimestamp = system_clock::now() + (strings::ToTimePoint(expiry, kTimestampFormat) - serverTime);

    auto serverPathsNode = document.select_node("Inrix/AuthResponse/ServerPaths").node();
    for (auto const & serverPathNode : serverPathsNode.children())
    {
      string const type = serverPathNode.attribute("type").as_string("");
      if (type == "API")
      {
        string const regionStr = serverPathNode.attribute("region").as_string("");
        if (find(kRegionIds.begin(), kRegionIds.end(), regionStr) != kRegionIds.end())
          m_serverUrls[regionStr] = serverPathNode.text().as_string("");
      }
    }
    m_authRequest.reset();

    if (m_deferredCallback != nullptr)
    {
      GetTrafficInternal(m_deferredCallback);
      m_deferredCallback = nullptr;
    }
  };

  m_authRequest.reset(downloader::HttpRequest::Get(kGetSecurityToken, callback));
}

void InrixApi::GetTraffic(TTrafficCallback const & trafficCallback)
{
  if (trafficCallback == nullptr)
    return;

  // Check authentication.
  {
    lock_guard<mutex> lock(m_mutex);
    bool const isTokenExpired = ((system_clock::now() - m_expirationTimestamp) < minutes(5));
    if (m_authToken.empty()  || isTokenExpired)
    {
      m_deferredCallback = trafficCallback;
      Authenticate();
      return;
    }
  }

  GetTrafficInternal(trafficCallback);
}

void InrixApi::GetTrafficInternal(TTrafficCallback const & trafficCallback)
{
  string const kMoscowGeoId = "306";
  string const url = MakeApiUrl("GetSegmentSpeedInGeography", {{"GeoId", kMoscowGeoId},
                                                               {"SpeedOutputFields", "Reference,Speed"},
                                                               {"FRCLevel", "1,2"},
                                                               {"Units", "1"}});
  auto const callback = [this, trafficCallback](downloader::HttpRequest & answer)
  {
    string const & responseStr = answer.Data();
    pugi::xml_document document;
    document.load_buffer(responseStr.c_str(), responseStr.length());
    if (!document)
      return;
    vector<df::TrafficSegmentData> data;
    auto reportNode = document.select_node("Inrix/SegmentSpeedResultSet/SegmentSpeedResults").node();
    for (pugi::xml_node const & segmentNode : reportNode.children("Segment"))
    {
      string const id = segmentNode.attribute("code").as_string();
      int const speed = segmentNode.attribute("speed").as_int();
      int const refSpeed = segmentNode.attribute("reference").as_int();
      data.push_back(df::TrafficSegmentData(id, GetSpeedBucket(speed, refSpeed)));
    }
    trafficCallback(data);
    m_trafficRequest.reset();
  };

  m_trafficRequest.reset(downloader::HttpRequest::Get(url, callback));
}

vector<InrixApi::SegmentData> InrixApi::GetSegments()
{
#ifdef INRIX_PARSE_ORIGINAL
  int const kMaxRoadClass = 7;
  int const kMaxFormOfWay = 9;
#endif

  vector<InrixApi::SegmentData> segments;

  pugi::xml_document document;

  string content;
  try
  {
    auto reader = GetPlatform().GetReader("inrix_segments.dat");
    reader->ReadAsString(content);
  }
  catch (RootException const & e)
  {
    LOG(LWARNING, ("Could not read: ", e.what()));
    return segments;
  }

  pugi::xml_parse_result result = document.load_buffer(content.data(), content.size());
  if (!result)
  {
    LOG(LWARNING, ("Could not parse: ", result.description()));
    return segments;
  }

#ifdef INRIX_PACK_SEGMENTS
  pugi::xml_document outDocument;
  auto outSegmentsNode = outDocument.append_child("Segments");
#endif

#ifdef INRIX_PARSE_ORIGINAL
  auto reportNode = document.select_node("Inrix/Dictionary/Report").node();
  for (auto const & seg : reportNode.children())
  {
    vector<m2::PointD> points;
    int roadClass = 7;
    int formOfWay = 9;
    string const segmentId = seg.child("ReportSegmentID").text().as_string("");
    pugi::xml_node locRef = seg.select_node("ReportSegmentLRC/method/olr:locationReference").node();
    {
      pugi::xml_node geomNode = locRef.child("olr:optionLinearLocationReference");
      pugi::xml_node firstCoord = geomNode.select_node("olr:first/olr:coordinate").node();
      auto longitude = firstCoord.child("olr:longitude").text().as_llong();
      auto latitude = firstCoord.child("olr:latitude").text().as_llong();
      ms::LatLon p1 = ms::LatLon(ConvertFirstCoordinate(latitude), ConvertFirstCoordinate(longitude));
      points.push_back(MercatorBounds::FromLatLon(p1));
      long long dnpFirst = geomNode.select_node("olr:first/olr:pathProperties/olr:dnp/olr:value").node().text().as_llong();
      long long dnpLast = dnpFirst;

      roadClass = min(roadClass, geomNode.select_node("olr:first/olr:lineProperties/olr:frc").node().attribute("olr:code").as_int(7));
      formOfWay = min(formOfWay, geomNode.select_node("olr:first/olr:lineProperties/olr:fow").node().attribute("olr:code").as_int(9));

      long long positiveOffset = 0;
      pugi::xml_node positiveOffsetNode = geomNode.select_node("olr:positiveOffset").node();
      if (positiveOffsetNode != nullptr)
        positiveOffset = positiveOffsetNode.child("olr:value").text().as_llong();

      long long negativeOffset = 0;
      pugi::xml_node negativeOffsetNode = geomNode.select_node("olr:negativeOffset").node();
      if (negativeOffsetNode != nullptr)
        negativeOffset = negativeOffsetNode.child("olr:value").text().as_llong();

      ms::LatLon lastPt = p1;
      for (auto const & intermediateNode : geomNode.select_nodes("olr:intermediates"))
      {
        pugi::xml_node coord = intermediateNode.node().child("olr:coordinate");
        auto longitudePt = coord.child("olr:longitude").text().as_llong();
        auto latitudePt = coord.child("olr:latitude").text().as_llong();
        ms::LatLon p = ms::LatLon(ConvertLastCoordinate(lastPt.lat, latitudePt),
                                  ConvertLastCoordinate(lastPt.lon, longitudePt));
        points.push_back(MercatorBounds::FromLatLon(p));
        dnpLast = intermediateNode.node().select_node("olr:pathProperties/olr:dnp/olr:value").node().text().as_llong();
        lastPt = p;

        roadClass = min(roadClass, intermediateNode.node().select_node("olr:lineProperties/olr:frc").node().attribute("olr:code").as_int(7));
        formOfWay = min(formOfWay, intermediateNode.node().select_node("olr:lineProperties/olr:fow").node().attribute("olr:code").as_int(9));
      }

      pugi::xml_node lastCoord = geomNode.select_node("olr:last/olr:coordinate").node();
      auto longitudeLast = lastCoord.child("olr:longitude").text().as_llong();
      auto latitudeLast = lastCoord.child("olr:latitude").text().as_llong();
      ms::LatLon p2 = ms::LatLon(ConvertLastCoordinate(lastPt.lat, latitudeLast),
                                 ConvertLastCoordinate(lastPt.lon, longitudeLast));
      points.push_back(MercatorBounds::FromLatLon(p2));

      roadClass = min(roadClass, geomNode.select_node("olr:last/olr:lineProperties/olr:frc").node().attribute("olr:code").as_int(7));
      formOfWay = min(formOfWay, geomNode.select_node("olr:last/olr:lineProperties/olr:fow").node().attribute("olr:code").as_int(9));

      if (roadClass > kMaxRoadClass || formOfWay > kMaxFormOfWay)
        continue;

      if (points.size() == 2 && (positiveOffset + negativeOffset) >= dnpFirst)
      {
        LOG(LWARNING, ("Segment degeneration"));
        continue;
      }

      m2::PointD newStart;
      if (positiveOffset != 0)
      {
        double const t = static_cast<double>(positiveOffset) / dnpFirst;
        if (t < 1.0)
        {
          newStart = points[0] * (1 - t) + points[1] * t;
        }
        else
        {
          LOG(LWARNING, ("Incorrect positive offset"));
          newStart = points[0];
          continue;
        }
      }

      m2::PointD newEnd;
      if (negativeOffset != 0)
      {
        ASSERT_GREATER_OR_EQUAL(points.size(), 2, ());
        size_t const lastIndex = points.size() - 1;
        double const t = static_cast<double>(negativeOffset) / dnpLast;
        if (t < 1.0)
        {
          newEnd = points[lastIndex] * (1 - t) + points[lastIndex - 1] * t;
        }
        else
        {
          LOG(LWARNING, ("Incorrect negative offset"));
          newEnd = points[lastIndex];
          continue;
        }
      }

      if (positiveOffset != 0)
        points[0] = newStart;

      if (negativeOffset != 0)
        points[points.size() - 1] = newEnd;
    }

#ifdef INRIX_PACK_SEGMENTS
    auto segNode = outSegmentsNode.append_child("Segment");
    segNode.append_attribute("id").set_value(segmentId.c_str());
    for (auto const & pt : points)
    {
      auto ptNode = segNode.append_child("Point");
      ptNode.append_attribute("x").set_value(pt.x);
      ptNode.append_attribute("y").set_value(pt.y);
    }
#endif
    segments.push_back({ segmentId, points });
  }
#else
  auto reportNode = document.select_node("Segments").node();
  for (auto const & seg : reportNode.children())
  {
    vector<m2::PointD> points;
    points.reserve(2);
    string const segmentId = seg.attribute("id").as_string("");
    for (auto const & ptNode : seg.children("Point"))
      points.push_back(m2::PointD(ptNode.attribute("x").as_double(), ptNode.attribute("y").as_double()));
    segments.push_back({ segmentId, points });
  }
#endif

  return segments;
}

string InrixApi::MakeApiUrl(string const & action,
                            initializer_list<pair<string, string>> const & params)
{
  stringstream ss;
  ss << m_serverUrls[m_regionId] << "?Action=" << action << "&Token=" << UrlEncode(m_authToken);
  for (auto const & param : params)
    ss << "&" << param.first << "=" << param.second;
  return ss.str();
}
