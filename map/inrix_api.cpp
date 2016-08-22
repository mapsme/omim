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

string const kGetSecurityToken = "http://api.inrix.com/Traffic/Inrix.ashx?" \
                                 "Action=GetSecurityToken&VendorID=" \
                                 INRIX_VENDOR_ID \
                                 "&ConsumerID=" \
                                 INRIX_CONSUMER_ID;

vector<string> const kRegionIds = { "EU", "NA" };

double ConvertFirstCoordinate(long long val)
{
  double const s = (val > 0) ? 1 : ((val < 0) ? -1 : 0);
  return (((double)val - 0.5 * s) * 360.0) / (1 << 24);
}

double ConvertLastCoordinate(double degVal, long long val)
{
  return degVal + (double)val / 100000.0;
}

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

    string const expiry = authTokenNode.attribute("expiry").as_string("");
    m_expirationTimestamp = strings::ToTimePoint(expiry, "%FT%TZ");

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
    if (m_authToken.empty() /* || deprecated */)
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
  int const kMaxRoadClass = 7;
  int const kMaxFormOfWay = 9;

  vector<InrixApi::SegmentData> segments;

  pugi::xml_document document;
  pugi::xml_parse_result result = document.load_file((GetPlatform().ResourcesDir() + "/inrix_segments.xml").c_str());
  if (!result)
  {
    LOG(LWARNING, ("Could not read inrix_segments.xml.", result.description()));
    return segments;
  }

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

    segments.push_back({ segmentId, points });
  }
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
