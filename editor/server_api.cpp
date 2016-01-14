#include "editor/server_api.hpp"

#include "coding/url_encode.hpp"

#include "geometry/mercator.hpp"

#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include "std/sstream.hpp"

namespace osm
{

ServerApi06::ServerApi06(OsmOAuth & auth)
  : m_auth(auth)
{
}

bool ServerApi06::CreateChangeSet(TKeyValueTags const & kvTags, uint64_t & outChangeSetId) const
{
  if (!m_auth.IsAuthorized())
  {
    LOG(LWARNING, ("Not authorized"));
    return false;
  }

  ostringstream stream;
  stream << "<osm>\n"
  "<changeset>\n";
  for (auto const & tag : kvTags)
    stream << "  <tag k=\"" << tag.first << "\" v=\"" << tag.second << "\"/>\n";
  stream << "</changeset>\n"
  "</osm>\n";

  OsmOAuth::Response const response = m_auth.Request("/changeset/create", "PUT", move(stream.str()));
  if (response.first == OsmOAuth::ResponseCode::OK)
  {
    if (strings::to_uint64(response.second, outChangeSetId))
      return true;
    LOG(LWARNING, ("Can't parse changeset ID from server response."));
  }
  else
    LOG(LWARNING, ("CreateChangeSet request has failed:", response.first));

  return false;
}

bool ServerApi06::CreateNode(string const & nodeXml, uint64_t & outCreatedNodeId) const
{
  OsmOAuth::Response const response = m_auth.Request("/node/create", "PUT", move(nodeXml));
  if (response.first == OsmOAuth::ResponseCode::OK)
  {
    if (strings::to_uint64(response.second, outCreatedNodeId))
      return true;
    LOG(LWARNING, ("Can't parse created node ID from server response."));
  }
  else
    LOG(LWARNING, ("CreateNode request has failed:", response.first));

  return false;
}

bool ServerApi06::ModifyNode(string const & nodeXml, uint64_t nodeId) const
{
  OsmOAuth::Response const response = m_auth.Request("/node/" + strings::to_string(nodeId), "PUT", move(nodeXml));
  if (response.first == OsmOAuth::ResponseCode::OK)
    return true;

  LOG(LWARNING, ("ModifyNode request has failed:", response.first));
  return false;
}

ServerApi06::DeleteResult ServerApi06::DeleteNode(string const & nodeXml, uint64_t nodeId) const
{
  OsmOAuth::Response const response = m_auth.Request("/node/" + strings::to_string(nodeId), "DELETE", move(nodeXml));
  if (response.first == OsmOAuth::ResponseCode::OK)
    return DeleteResult::ESuccessfullyDeleted;
  else if (static_cast<int>(response.first) >= 400)
    // Tons of reasons, see http://wiki.openstreetmap.org/wiki/API_v0.6#Error_codes_16
    return DeleteResult::ECanNotBeDeleted;

  LOG(LWARNING, ("DeleteNode request has failed:", response.first));
  return DeleteResult::EFailed;
}

bool ServerApi06::CloseChangeSet(uint64_t changesetId) const
{
  OsmOAuth::Response const response = m_auth.Request("/changeset/" + strings::to_string(changesetId) + "/close", "PUT");
  if (response.first == OsmOAuth::ResponseCode::OK)
    return true;

  LOG(LWARNING, ("CloseChangeSet request has failed:", response.first));
  return false;
}

bool ServerApi06::TestUserExists(string const & userName)
{
  string const method = "/user/" + UrlEncode(userName);
  OsmOAuth::Response const response = m_auth.DirectRequest(method, false);
  return response.first == OsmOAuth::ResponseCode::OK;
}

string ServerApi06::GetXmlFeaturesInRect(m2::RectD const & latLonRect) const
{
  using strings::to_string_dac;

  // Digits After Comma.
  static constexpr double const kDAC = 7;
  m2::PointD const lb = latLonRect.LeftBottom();
  m2::PointD const rt = latLonRect.RightTop();
  string const url = "/map?bbox=" + to_string_dac(lb.x, kDAC) + ',' + to_string_dac(lb.y, kDAC) + ',' +
      to_string_dac(rt.x, kDAC) + ',' + to_string_dac(rt.y, kDAC);

  OsmOAuth::Response const response = m_auth.DirectRequest(url);
  if (response.first == OsmOAuth::ResponseCode::OK)
    return response.second;

  LOG(LWARNING, ("GetXmlFeaturesInRect request has failed:", response.first));
  return string();
}

string ServerApi06::GetXmlNodeByLatLon(double lat, double lon) const
{
  constexpr double const kInflateRadiusDegrees = 1.0e-6; //!< ~1 meter.
  m2::RectD rect(lon, lat, lon, lat);
  rect.Inflate(kInflateRadiusDegrees, kInflateRadiusDegrees);
  return GetXmlFeaturesInRect(rect);
}

} // namespace osm
