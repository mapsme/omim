#pragma once
#include "transit/world_feed/color_picker.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"

#include <cstdint>
#include <fstream>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "3party/just_gtfs/just_gtfs.h"
#include "3party/opening_hours/opening_hours.hpp"

#include "defines.hpp"

namespace transit
{
class WorldFeedIntegrationTests;

// File names for saving resulting data exported from GTFS.
inline std::string const kTransitFileExtension = std::string(TRANSIT_FILE_EXTENSION);
inline std::string const kNetworksFile = "networks" + kTransitFileExtension;
inline std::string const kRoutesFile = "routes" + kTransitFileExtension;
inline std::string const kLinesFile = "lines" + kTransitFileExtension;
inline std::string const kShapesFile = "shapes" + kTransitFileExtension;
inline std::string const kStopsFile = "stops" + kTransitFileExtension;
inline std::string const kEdgesFile = "edges" + kTransitFileExtension;
inline std::string const kEdgesTransferFile = "edges_transfer" + kTransitFileExtension;
inline std::string const kTransfersFile = "transfers" + kTransitFileExtension;
inline std::string const kGatesFile = "gates" + kTransitFileExtension;

// Unique id persistent between re-runs. Generated based on the unique string hash of the
// GTFS entity. Lies in the interval |routing::FakeFeatureIds::IsTransitFeature()|.
// If the GTFS entity is renamed or the new GTFS feed is added the new id is generated by
// |IdGenerator::MakeId()|.
using TransitId = uint32_t;

// Generates globally unique TransitIds mapped to the GTFS entities hashes.
class IdGenerator
{
public:
  explicit IdGenerator(std::string const & idMappingPath);
  void Save();

  TransitId MakeId(std::string const & hash);

private:
  std::unordered_map<std::string, TransitId> m_hashToId;
  TransitId m_curId;
  std::string m_idMappingPath;
};

// Here are MAPS.ME representations for GTFS entities, e.g. networks for GTFS agencies.
// https://developers.google.com/transit/gtfs/reference

// Mapping of language id to text.
using Translations = std::unordered_map<std::string, std::string>;

struct Networks
{
  void Write(std::ofstream & stream) const;

  // Id to agency name mapping.
  std::unordered_map<TransitId, Translations> m_data;
};

struct RouteData
{
  TransitId m_networkId = 0;
  std::string m_routeType;
  Translations m_title;
  std::string m_color;
};

struct Routes
{
  void Write(std::ofstream & stream) const;

  std::unordered_map<TransitId, RouteData> m_data;
};

struct LineInterval
{
  size_t m_headwayS = 0;
  osmoh::OpeningHours m_timeIntervals;
};

using LineIntervals = std::vector<LineInterval>;
using IdList = std::vector<TransitId>;

// Link to the shape: its id and indexes in the corresponding polyline.
struct ShapeLink
{
  TransitId m_shapeId = 0;
  size_t m_startIndex = 0;
  size_t m_endIndex = 0;
};

struct LineData
{
  TransitId m_routeId = 0;
  ShapeLink m_shapeLink;
  Translations m_title;
  // Sequence of stops along the line from first to last.
  IdList m_stopIds;

  // Transport intervals depending on the day timespans.
  std::vector<LineInterval> m_intervals;
  // Monthdays and weekdays ranges on which the line is at service.
  // Exceptions in service schedule. Explicitly activates or disables service by dates.
  osmoh::OpeningHours m_serviceDays;

  // Fields not intended to be exported to json.
  TransitId m_shapeId = 0;
  std::string m_gtfsTripId;
  std::string m_gtfsServiceId;
};

struct Lines
{
  void Write(std::ofstream & stream) const;

  std::unordered_map<TransitId, LineData> m_data;
};

struct ShapeData
{
  ShapeData() = default;
  explicit ShapeData(std::vector<m2::PointD> const & points);

  std::vector<m2::PointD> m_points;
  // Field not for dumping to json:
  std::unordered_set<TransitId> m_lineIds;
};

struct Shapes
{
  void Write(std::ofstream & stream) const;

  std::unordered_map<TransitId, ShapeData> m_data;
};

struct StopData
{
  void UpdateTimetable(TransitId lineId, gtfs::StopTime const & stopTime);

  m2::PointD m_point;
  Translations m_title;
  // For each line id there is a schedule.
  std::unordered_map<TransitId, osmoh::OpeningHours> m_timetable;

  // Field not intended for dumping to json:
  std::string m_gtfsParentId;
};

struct Stops
{
  void Write(std::ofstream & stream) const;

  std::unordered_map<TransitId, StopData> m_data;
};

struct EdgeId
{
  EdgeId() = default;
  EdgeId(TransitId fromStopId, TransitId toStopId, TransitId lineId);

  bool operator==(EdgeId const & other) const;

  TransitId m_fromStopId = 0;
  TransitId m_toStopId = 0;
  TransitId m_lineId = 0;
};

struct EdgeIdHasher
{
  size_t operator()(EdgeId const & key) const;
};

struct EdgeData
{
  ShapeLink m_shapeLink;
  size_t m_weight = 0;
};

struct Edges
{
  void Write(std::ofstream & stream) const;

  std::unordered_map<EdgeId, EdgeData, EdgeIdHasher> m_data;
};

struct EdgeTransferData
{
  TransitId m_fromStopId = 0;
  TransitId m_toStopId = 0;
  size_t m_weight = 0;
};

bool operator<(EdgeTransferData const & d1, EdgeTransferData const & d2);

struct EdgesTransfer
{
  void Write(std::ofstream & stream) const;

  std::set<EdgeTransferData> m_data;
};

struct TransferData
{
  m2::PointD m_point;
  IdList m_stopsIds;
};

struct Transfers
{
  void Write(std::ofstream & stream) const;

  std::unordered_map<TransitId, TransferData> m_data;
};

struct TimeFromGateToStop
{
  TransitId m_stopId = 0;
  size_t m_timeSeconds = 0;
};

struct GateData
{
  bool m_isEntrance = false;
  bool m_isExit = false;
  m2::PointD m_point;
  std::vector<TimeFromGateToStop> m_weights;

  // Field not intended for dumping to json:
  std::string m_gtfsId;
};

struct Gates
{
  void Write(std::ofstream & stream) const;

  std::unordered_map<TransitId, GateData> m_data;
};

// Indexes for WorldFeed |m_gtfsIdToHash| field. For each type of GTFS entity, e.g. agency or stop,
// there is distinct mapping located by its own |FieldIdx| index in the |m_gtfsIdToHash|.
enum FieldIdx
{
  AgencyIdx = 0,
  StopsIdx,
  RoutesIdx,
  TripsIdx,
  ShapesIdx,
  IdxCount
};

using GtfsIdToHash = std::unordered_map<std::string, std::string>;
using CalendarCache = std::unordered_map<std::string, osmoh::TRuleSequences>;

struct StopsOnLines
{
  explicit StopsOnLines(IdList const & ids);

  IdList m_stopSeq;
  std::unordered_set<TransitId> m_lines;
  bool m_isValid = true;
};

// Class for merging scattered GTFS feeds into one World feed with static ids.
class WorldFeed
{
public:
  WorldFeed(IdGenerator & generator, ColorPicker & colorPicker);
  // Transforms GTFS feed into the global feed.
  bool SetFeed(gtfs::Feed && feed);

  // Dumps global feed to |world_feed_path|.
  bool Save(std::string const & worldFeedDir, bool overwrite);

  inline static size_t GetCorruptedStopSequenceCount() { return m_badStopSeqCount; }

private:
  friend class WorldFeedIntegrationTests;

  bool SetFeedLanguage();
  // Fills networks from GTFS agencies data.
  bool FillNetworks();
  // Fills routes from GTFS foutes data.
  bool FillRoutes();
  // Fills lines and corresponding shapes from GTFS trips and shapes.
  bool FillLinesAndShapes();
  // Deletes shapes which are sub-shapes and refreshes corresponding links in lines.
  void ModifyLinesAndShapes();
  // Gets service monthday open/closed ranges, weekdays and exceptions in schedule.
  bool FillLinesSchedule();
  // Gets frequencies of trips from GTFS.

  // Adds shape with mercator points instead of WGS83 lat/lon.
  bool AddShape(GtfsIdToHash::iterator & iter, std::string const & gtfsShapeId, TransitId lineId);
  // Fills stops data and builds corresponding edges for the road graph.
  bool FillStopsEdges();

  // Generates globally unique id and hash for the stop by its |stopGtfsId|.
  std::pair<TransitId, std::string> GetStopIdAndHash(std::string const & stopGtfsId);

  // Adds new stop with |stopId| and fills it with GTFS data by |gtfsId| or just
  // links to it |lineId|.
  bool UpdateStop(TransitId stopId, gtfs::StopTime const & stopTime, std::string const & stopHash,
                  TransitId lineId);

  std::unordered_map<TransitId, std::vector<StopsOnLines>> GetStopsForShapeMatching();

  // Adds stops projections to shapes. Updates corresponding links to shapes.
  size_t ModifyShapes();
  // Fills transfers based on GTFS transfers.
  void FillTransfers();
  // Fills gates based on GTFS stops.
  void FillGates();

  bool ProjectStopsToShape(TransitId shapeId, std::vector<m2::PointD> & shape,
                           IdList const & stopIds,
                           std::unordered_map<TransitId, std::vector<size_t>> & stopsToIndexes);

  // Extracts data from GTFS calendar for lines.
  void GetCalendarDates(osmoh::TRuleSequences & rules, CalendarCache & cache,
                        std::string const & serviceId);
  // Extracts data from GTFS calendar dates for lines.
  void GetCalendarDatesExceptions(osmoh::TRuleSequences & rules, CalendarCache & cache,
                                  std::string const & serviceId);

  LineIntervals GetFrequencies(std::unordered_map<std::string, LineIntervals> & cache,
                               std::string const & tripId);
  // Current GTFS feed which is being merged to the global feed.
  gtfs::Feed m_feed;

  // Entities for json'izing and feeding to the generator_tool.
  Networks m_networks;
  Routes m_routes;
  Lines m_lines;
  Shapes m_shapes;
  Stops m_stops;
  Edges m_edges;
  EdgesTransfer m_edgesTransfers;
  Transfers m_transfers;
  Gates m_gates;

  // Generator of ids, globally unique and constant between re-runs.
  IdGenerator & m_idGenerator;
  // Color name picker of the nearest color for route RBG from our constant list of transfer colors.
  ColorPicker & m_colorPicker;

  // GTFS id -> entity hash mapping. Maps GTFS id string (unique only for current feed) to the
  // globally unique hash.
  std::vector<GtfsIdToHash> m_gtfsIdToHash;

  // Unique hash characterizing each GTFS feed.
  std::string m_gtfsHash;

  // Unique hashes of all agencies handled by WorldFeed.
  static std::unordered_set<std::string> m_agencyHashes;
  // Count of corrupted stops sequences which could not be projected to the shape polyline.
  static size_t m_badStopSeqCount;
  // Agencies which are already handled by WorldFeed and should be copied to the resulting jsons.
  std::unordered_set<std::string> m_agencySkipList;

  // If the feed explicitly specifies its language, we use its value. Otherwise set to default.
  std::string m_feedLanguage;
};
}  // namespace transit
