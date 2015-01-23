#include "map/mwm_url.hpp"

#include "drape_frontend/visual_params.hpp"

#include "indexer/mercator.hpp"
#include "indexer/scales.hpp"

#include "coding/uri.hpp"

#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include "std/algorithm.hpp"
#include "std/bind.hpp"


namespace url_scheme
{

namespace
{

static int const INVALID_LAT_VALUE = -1000;

bool IsInvalidApiPoint(ApiPoint const & p) { return p.m_lat == INVALID_LAT_VALUE; }

void ParseKeyValue(string key, string const & value, vector<ApiPoint> & points, ParsedMapApi & parsedApi)
{
  strings::AsciiToLower(key);

  if (key == "ll")
  {
    points.push_back(ApiPoint());
    points.back().m_lat = INVALID_LAT_VALUE;

    size_t const firstComma = value.find(',');
    if (firstComma == string::npos)
    {
      LOG(LWARNING, ("Map API: no comma between lat and lon for 'll' key", key, value));
      return;
    }

    if (value.find(',', firstComma + 1) != string::npos)
    {
      LOG(LWARNING, ("Map API: more than one comma in a value for 'll' key", key, value));
      return;
    }

    double lat, lon;
    if (!strings::to_double(value.substr(0, firstComma), lat) ||
        !strings::to_double(value.substr(firstComma + 1), lon))
    {
      LOG(LWARNING, ("Map API: can't parse lat,lon for 'll' key", key, value));
      return;
    }

    if (!MercatorBounds::ValidLat(lat) || !MercatorBounds::ValidLon(lon))
    {
      LOG(LWARNING, ("Map API: incorrect value for lat and/or lon", key, value, lat, lon));
      return;
    }

    points.back().m_lat = lat;
    points.back().m_lon = lon;
  }
  else if (key == "z")
  {
    if (!strings::to_double(value, parsedApi.m_zoomLevel))
      parsedApi.m_zoomLevel = 0.0;
  }
  else if (key == "n")
  {
    if (!points.empty())
      points.back().m_name = value;
    else
      LOG(LWARNING, ("Map API: Point name with no point. 'll' should come first!"));
  }
  else if (key == "id")
  {
    if (!points.empty())
      points.back().m_id = value;
    else
      LOG(LWARNING, ("Map API: Point url with no point. 'll' should come first!"));
  }
  else if (key == "backurl")
  {
    // Fix missing :// in back url, it's important for iOS
    if (value.find("://") == string::npos)
      parsedApi.m_globalBackUrl = value + "://";
    else
      parsedApi.m_globalBackUrl = value;
  }
  else if (key == "v")
  {
    if (!strings::to_int(value, parsedApi.m_version))
      parsedApi.m_version = 0;
  }
  else if (key == "appname")
    parsedApi.m_appTitle = value;
  else if (key == "balloonaction")
    parsedApi.m_goBackOnBalloonClick = true;
}

}  // unnames namespace

UserMark const * ParseUrl(UserMarksController & controller, string const & url,
                          ParsedMapApi & apiData, m2::RectD & rect)
{
  apiData = ParsedMapApi();
  auto beforeReturnOnFail = [&apiData, &rect]
  {
    apiData.m_isValid = false;
    rect.MakeEmpty();
  };

  url_scheme::Uri uri = url_scheme::Uri(url);
  string const & scheme = uri.GetScheme();
  if ((scheme != "mapswithme" && scheme != "mwm") || uri.GetPath() != "map")
  {
    beforeReturnOnFail();
    return nullptr;
  }

  vector<ApiPoint> points;
  uri.ForEachKeyValue(bind(&ParseKeyValue, _1, _2, ref(points), ref(apiData)));
  points.erase(remove_if(points.begin(), points.end(), &IsInvalidApiPoint), points.end());

  if (points.empty())
  {
    beforeReturnOnFail();
    return nullptr;
  }

  apiData.m_isValid = true;
  for (size_t i = 0; i < points.size(); ++i)
  {
    ApiPoint const & p = points[i];
    m2::PointD glPoint(MercatorBounds::FromLatLon(p.m_lat, p.m_lon));

    ApiMarkPoint * mark = static_cast<ApiMarkPoint *>(controller.CreateUserMark(glPoint));
    mark->SetName(p.m_name);
    mark->SetID(p.m_id);

    rect.Add(glPoint);
  }

  ASSERT(controller.GetUserMarkCount() > 0, ());
  if (controller.GetUserMarkCount() > 1)
    return nullptr;

  UserMark const * result = controller.GetUserMark(0);

  double zoom = min(static_cast<double>(scales::GetUpperComfortScale()), apiData.m_zoomLevel);
  rect = df::GetRectForDrawScale(zoom, result->GetOrg());
  if (!rect.IsValid())
    rect = df::GetWorldRect();

  return result;
}

}
