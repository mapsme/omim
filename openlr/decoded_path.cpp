#include "openlr/decoded_path.hpp"

#include "indexer/index.hpp"

#include "platform/country_file.hpp"

#include "geometry/latlon.hpp"
#include "geometry/mercator.hpp"

#include "base/assert.hpp"
#include "base/string_utils.hpp"

#define THROW_IF_NODE_IS_EMPTY(node, exc, msg) \
  if (!node)                                   \
    MYTHROW(exc, msg)

namespace
{
bool IsForwardFromXML(pugi::xml_node const & node)
{
  THROW_IF_NODE_IS_EMPTY(node, openlr::DecodedPathLoadError, ("Can't parse IsForward"));
  return node.text().as_bool();
}

uint32_t SegmentIdFromXML(pugi::xml_node const & node)
{
  THROW_IF_NODE_IS_EMPTY(node, openlr::DecodedPathLoadError, ("Can't parse SegmentId"));
  return node.text().as_uint();
}

void LatLonToXML(ms::LatLon const & latLon, pugi::xml_node & node)
{
  node.append_child("lat").text() = latLon.lat;
  node.append_child("lon").text() = latLon.lon;
}

void LatLonFromXML(pugi::xml_node const & node, ms::LatLon & latLon)
{
  THROW_IF_NODE_IS_EMPTY(node, openlr::DecodedPathLoadError, ("Can't parse latLon"));
  latLon.lat = node.child("lat").text().as_double();
  latLon.lon = node.child("lon").text().as_double();
}

void FeatureIdFromXML(pugi::xml_node const & node, Index const & index, FeatureID & fid)
{
  THROW_IF_NODE_IS_EMPTY(node, openlr::DecodedPathLoadError, ("Can't parse CountryName"));
  auto const countryName = node.child("CountryName").text().as_string();
  fid.m_mwmId = index.GetMwmIdByCountryFile(platform::CountryFile(countryName));
  fid.m_index = node.child("Index").text().as_uint();
}

void FeatureIdToXML(FeatureID const & fid, pugi::xml_node & node)
{
  node.append_child("CountryName").text() = fid.m_mwmId.GetInfo()->GetCountryName().data();
  node.append_child("Index").text() = fid.m_index;
}
}  // namespace

namespace openlr
{
void WriteAsMappingForSpark(std::string const & fileName, std::vector<DecodedPath> const & paths)
{
  std::ofstream ost(fileName);
  if (!ost.is_open())
    MYTHROW(DecodedPathSaveError, ("Can't write to file", fileName, strerror(errno)));

  WriteAsMappingForSpark(ost, paths);

  if (ost.fail())
  {
    MYTHROW(DecodedPathSaveError,
            ("An error occured while writing file", fileName, strerror(errno)));
  }
}

void WriteAsMappingForSpark(std::ostream & ost, std::vector<DecodedPath> const & paths)
{
  ost << std::fixed;  // Avoid scientific notation cause '-' is used as fields separator.
  for (auto const & p : paths)
  {
    if (p.m_path.empty())
      continue;

    ost << p.m_segmentId.Get() << '\t';

    auto const kFieldSep = '-';
    auto const kSegmentSep = '=';
    for (auto it = begin(p.m_path); it != end(p.m_path); ++it)
    {
      auto const & fid = it->GetFeatureId();
      ost << fid.m_mwmId.GetInfo()->GetCountryName()
          << kFieldSep << fid.m_index
          << kFieldSep << it->GetSegId()
          << kFieldSep << (it->IsForward() ? "fwd" : "bwd")
          << kFieldSep << MercatorBounds::DistanceOnEarth(GetStart(*it), GetEnd(*it));

      if (next(it) != end(p.m_path))
        ost << kSegmentSep;
    }
    ost << endl;
  }
}

void PathFromXML(pugi::xml_node const & node, Index const & index, Path & p)
{
  auto const edges = node.select_nodes("RoadEdge");
  for (auto const xmlE : edges)
  {
    auto e = xmlE.node();

    FeatureID fid;
    FeatureIdFromXML(e.child("FeatureID"), index, fid);

    auto const isForward = IsForwardFromXML(e.child("IsForward"));
    auto const segmentId = SegmentIdFromXML(e.child("SegmentId"));

    ms::LatLon start, end;
    LatLonFromXML(e.child("StartJunction"), start);
    LatLonFromXML(e.child("EndJunction"), end);

    p.emplace_back(fid, isForward, segmentId,
                   routing::Junction(MercatorBounds::FromLatLon(start), 0 /* altitude */),
                   routing::Junction(MercatorBounds::FromLatLon(end), 0 /* altitude */));
  }
}

void PathToXML(Path const & path, pugi::xml_node & node)
{
  for (auto const e : path)
  {
    auto edge = node.append_child("RoadEdge");

    {
      auto fid = edge.append_child("FeatureID");
      FeatureIdToXML(e.GetFeatureId(), fid);
    }

    edge.append_child("IsForward").text() = e.IsForward();
    edge.append_child("SegmentId").text() = e.GetSegId();
    {
      auto start = edge.append_child("StartJunction");
      auto end = edge.append_child("EndJunction");
      LatLonToXML(MercatorBounds::ToLatLon(GetStart(e)), start);
      LatLonToXML(MercatorBounds::ToLatLon(GetEnd(e)), end);
    }
  }
}
}  // namespace openlr

namespace routing
{
std::vector<m2::PointD> GetPoints(openlr::Path const & p)
{
  CHECK(!p.empty(), ("Path should not be empty"));
  std::vector<m2::PointD> points;
  points.push_back(GetStart(p.front()));
  for (auto const & e : p)
    points.push_back(GetEnd(e));

  return points;
}
}  // namespace routing
