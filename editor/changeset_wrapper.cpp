#include "base/logging.hpp"

#include "indexer/feature.hpp"

#include "editor/changeset_wrapper.hpp"
#include "editor/osm_utils.hpp"

#include "std/algorithm.hpp"
#include "std/sstream.hpp"

#include "geometry/mercator.hpp"

#include "private.h"

using editor::XMLFeature;

namespace
{
double scoreLatLon(pugi::xml_node const & node, ms::LatLon const & latLon)
{
  return 0;
}

double scoreNames(pugi::xml_node const & mode, StringUtf8Multilang const & names)
{
  return 0;
}

double scoreMetadata(pugi::xml_node const & node, feature::Metadata const & metadata)
{
  return 0;
}

bool TypesEqual(pugi::xml_node const & node, feature::TypesHolder const & types)
{
  return false;
}

pugi::xml_node GetBestOsmNode(pugi::xml_document const & osmRespose, FeatureType const & ft)
{
  double bestScore = -1;
  pugi::xml_node bestMatchNode;

  for (auto const & xNode : osmRespose.select_nodes("node"))
  {
    try
    {
      auto const & node = xNode.node();

      if (!TypesEqual(node, feature::TypesHolder(ft)))
        continue;

      double nodeScore = scoreLatLon(node, MercatorBounds::ToLatLon(ft.GetCenter()));
      if (nodeScore < 0)
        continue;

      nodeScore += scoreNames(node, ft.GetNames());
      nodeScore += scoreMetadata(node, ft.GetMetadata());

      if (bestScore < nodeScore)
      {
        bestScore = nodeScore;
        bestMatchNode = node;
      }
    }
    catch (editor::utils::NoLatLonError)
    {
      LOG(LWARNING, ("No lat/lon attribute in osm response node."));
      continue;
    }
  }

  // if (bestScore < minimumScoreThreashold)
  //   return pugi::xml_node;

  return bestMatchNode;
}
}  // namespace

string DebugPrint(pugi::xml_document const & doc)
{
  ostringstream stream;
  doc.print(stream, "  ");
  return stream.str();
}

namespace osm
{

ChangesetWrapper::ChangesetWrapper(TKeySecret const & keySecret,
                                   ServerApi06::TKeyValueTags const & comments)
  : m_changesetComments(comments),
    m_api(OsmOAuth::ServerAuth().SetToken(keySecret))
{
}

ChangesetWrapper::~ChangesetWrapper()
{
  if (m_changesetId)
    m_api.CloseChangeSet(m_changesetId);
}

void ChangesetWrapper::LoadXmlFromOSM(ms::LatLon const & ll, pugi::xml_document & doc)
{
  auto const response = m_api.GetXmlFeaturesAtLatLon(ll.lat, ll.lon);
  if (response.first == OsmOAuth::ResponseCode::NetworkError)
    MYTHROW(NetworkErrorException, ("NetworkError with GetXmlFeaturesAtLatLon request."));
  if (response.first != OsmOAuth::ResponseCode::OK)
    MYTHROW(HttpErrorException, ("HTTP error", response.first, "with GetXmlFeaturesAtLatLon", ll));

  if (pugi::status_ok != doc.load(response.second.c_str()).status)
    MYTHROW(OsmXmlParseException, ("Can't parse OSM server response for GetXmlFeaturesAtLatLon request", response.second));
}

XMLFeature ChangesetWrapper::GetMatchingFeatureFromOSM(XMLFeature const & ourPatch, FeatureType const & feature)
{
  if (feature.GetFeatureType() == feature::EGeomType::GEOM_POINT)
  {
    // Match with OSM node.
    ms::LatLon const ll = ourPatch.GetCenter();
    pugi::xml_document doc;
    // Throws!
    LoadXmlFromOSM(ll, doc);

    // feature must be the original one, not patched!
    pugi::xml_node const firstNode = GetBestOsmNode(doc, feature);
    if (firstNode.empty())
      MYTHROW(OsmObjectWasDeletedException, ("OSM does not have any nodes at the coordinates", ll, ", server has returned:", doc));

    return XMLFeature(firstNode);
  }
  else if (feature.GetFeatureType() == feature::EGeomType::GEOM_AREA)
  {
    using m2::PointD;
    // Set filters out duplicate points for closed ways or triangles' vertices.
    set<PointD> geometry;
    feature.ForEachTriangle([&geometry](PointD const & p1, PointD const & p2, PointD const & p3)
    {
      geometry.insert(p1);
      geometry.insert(p2);
      geometry.insert(p3);
    }, FeatureType::BEST_GEOMETRY);

    ASSERT_GREATER_OR_EQUAL(geometry.size(), 3, ("Is it an area feature?"));

    for (auto const & pt : geometry)
    {
      ms::LatLon const ll = MercatorBounds::ToLatLon(pt);
      pugi::xml_document doc;
      // Throws!
      LoadXmlFromOSM(ll, doc);

      // TODO(AlexZ): Select best matching OSM way from possible many ways.
      pugi::xml_node const firstWay = doc.child("osm").child("way");
      if (firstWay.empty())
        continue;

      XMLFeature const way(firstWay);
      if (!way.IsArea())
        continue;

      // TODO: Check that this way is really match our feature.

      return way;
    }
    MYTHROW(OsmObjectWasDeletedException, ("OSM does not have any matching way for feature", feature));
  }
  MYTHROW(LinearFeaturesAreNotSupportedException, ("We don't edit linear features yet."));
}

void ChangesetWrapper::ModifyNode(XMLFeature node)
{
  // TODO(AlexZ): ServerApi can be much better with exceptions.
  if (m_changesetId == kInvalidChangesetId && !m_api.CreateChangeSet(m_changesetComments, m_changesetId))
    MYTHROW(CreateChangeSetFailedException, ("CreateChangeSetFailedException"));

  uint64_t nodeId;
  if (!strings::to_uint64(node.GetAttribute("id"), nodeId))
    MYTHROW(CreateChangeSetFailedException, ("CreateChangeSetFailedException"));

  // Changeset id should be updated for every OSM server commit.
  node.SetAttribute("changeset", strings::to_string(m_changesetId));

  if (!m_api.ModifyNode(node.ToOSMString(), nodeId))
    MYTHROW(ModifyNodeFailedException, ("ModifyNodeFailedException"));
}

} // namespace osm
