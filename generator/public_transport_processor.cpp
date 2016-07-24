#include "generator/public_transport_processor.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

#include "defines.hpp"

// Stops are nodes with tags:
//  highway = bus_stop
//  amenity = bus_station
//  public_transport = stop_station

// Routes are relations with tags:
//  route = bus
//  route = tram
//  route = trolleybus
// relation |ref| tag is a route name

namespace
{
// Node keys
string const kAmenity = "amenity";
string const kHighway = "highway";
string const kPublicTransport = "public_transport";

// Node values
string const kBusStop = "bus_stop";
string const kBusStation = "bus_station";
string const kStopPosition = "stop_position";

// Relation keys
string const kRef = "ref";
string const kRoute = "route";

// Relation values
string const kBus = "bus";
string const kTram = "tram";
string const kTrolleybus = "trolleybus";
}  // namespace

bool PublicTransportProcessor::ProcessStop(OsmElement & node, FeatureParams & params)
{
  ASSERT_EQUAL(node.type, OsmElement::EntityType::Node, ());

  if (node.GetTag(kPublicTransport) != kStopPosition &&
      node.GetTag(kHighway) != kBusStop &&
      node.GetTag(kAmenity) != kBusStation)
  {
    return false; // not a stop
  }

  StopInfo & si = m_stops[node.id];
  si.m_params = params;
  si.m_pt = MercatorBounds::FromLatLon(node.lat, node.lon);

  return true;
}

bool PublicTransportProcessor::ProcessRoute(OsmElement & relation)
{
  ASSERT_EQUAL(relation.type, OsmElement::EntityType::Relation, ());

  string const route = relation.GetTag(kRoute);
  bool const isBus = route == kBus;
  bool const isTram = route == kTram;
  bool const isTrolleybus = route == kTrolleybus;

  if (!isBus && !isTram && !isTrolleybus)
    return false; // not a route

  string const ref = relation.GetTag(kRef);
  if (ref.empty())
    return false; // no route name, skip it

  for (OsmElement::Member const & m : relation.Members())
  {
    if (m.type != OsmElement::EntityType::Node)
      continue;

    RoutesInfo & routes = m_routesInNode[m.ref];

    if (isBus)
      routes.m_buses.insert(ref);
    else if (isTram)
      routes.m_trams.insert(ref);
    else if (isTrolleybus)
      routes.m_trolleybuses.insert(ref);
  }

  return true;
}

void PublicTransportProcessor::ForEachStop(function<void(osm::Id const & id,
                                                         m2::PointD const & pt,
                                                         FeatureParams const & params)> const & fn)
{
  size_t busStops = 0;
  size_t tramStops = 0;
  size_t trolleybusStops = 0;

  for (auto & kv : m_stops)
  {
    uint64_t const id = kv.first;
    StopInfo & si = kv.second;

    auto const i = m_routesInNode.find(id);
    if (i != m_routesInNode.end())
    {
      RoutesInfo const & routes = i->second;

      if (SetRoutesMetadata(si, feature::Metadata::FMD_BUS_ROUTES, routes.m_buses))
        ++busStops;

      if (SetRoutesMetadata(si, feature::Metadata::FMD_TRAM_ROUTES, routes.m_trams))
        ++tramStops;

      if (SetRoutesMetadata(si, feature::Metadata::FMD_TROLLEYBUS_ROUTES, routes.m_trolleybuses))
        ++trolleybusStops;
    }

    fn(osm::Id::Node(id), si.m_pt, si.m_params);
  }

  LOG(LINFO, ("Proccessed stops:", m_stops.size(),
              "Bus stops:", busStops,
              "Tram stops:", tramStops,
              "Trolleybus stops:", trolleybusStops));
}

void PublicTransportProcessor::Clear()
{
  m_stops.clear();
  m_routesInNode.clear();
}

bool PublicTransportProcessor::SetRoutesMetadata(StopInfo & si, feature::Metadata::EType key,
                                                 set<string> const & routes)
{
  if (routes.empty())
    return false;
  string value = strings::JoinStrings(routes, ROUTES_DELIMITER);
  si.m_params.GetMetadata().Set(key, value);
  return true;
}
