#pragma once

#include "generator/osm_element.hpp"
#include "generator/osm_id.hpp"

#include "indexer/feature_data.hpp"

#include "std/functional.hpp"
#include "std/map.hpp"
#include "std/set.hpp"

class PublicTransportProcessor
{
public:
  // Proccesses a node osm element.
  // Returns true if osm element is a stop, otherwise returns false.
  bool ProcessStop(OsmElement & node, FeatureParams & params);

  // Processes a relation osm element
  // Returns true if osm element is a route relation,
  // otherwise returns false.
  bool ProcessRoute(OsmElement & relation);

  // Enumerates stop features.
  // If bus routes are through a stop feature, FMD_BUS_ROUTES meta is set
  // and contains collection of bus routes, delimited by ROUTES_DELIMITER.
  // The same for tram stop - the FMD_TRAM_ROUTES is set, and
  // for trolleybus stops the FMD_THROLLEYBUS_ROUTES is set.
  void ForEachStop(function<void(osm::Id const & id,
                                 m2::PointD const & point,
                                 FeatureParams const & params)> const & fn);

  void Clear();

private:
  struct RoutesInfo
  {
    set<string> m_buses;
    set<string> m_trams;
    set<string> m_trolleybuses;
  };

  struct StopInfo
  {
    FeatureParams m_params;
    m2::PointD m_pt;
  };

  static bool SetRoutesMetadata(StopInfo & si, feature::Metadata::EType key,
                                set<string> const & routes);

  map<uint64_t, StopInfo> m_stops;
  unordered_map<uint64_t, RoutesInfo> m_routesInNode;
};
