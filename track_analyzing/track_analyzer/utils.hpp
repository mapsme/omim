#pragma once

#include "storage/storage.hpp"

#include "routing_common/num_mwm_id.hpp"

#include "track_analyzing/track.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <string>

namespace track_analyzing
{
class Stats
{
public:
  friend std::string DebugPrint(Stats const & s);

  using NameToCountMapping = std::map<std::string, uint64_t>;

  Stats() = default;
  Stats(NameToCountMapping const & mwmToTotalDataPoints,
        NameToCountMapping const & countryToTotalDataPoint);

  bool operator==(Stats const & stats) const;
  void Add(Stats const & stats);

  /// \brief Adds some stats according to |mwmToTracks|.
  void AddTracksStats(MwmToTracks const & mwmToTracks, routing::NumMwmIds const & numMwmIds,
                      storage::Storage const & storage);

  /// \brief Adds |dataPointNum| to |m_mwmToTotalDataPoints| and |m_countryToTotalDataPoints|.
  void AddDataPoints(std::string const & mwmName, std::string const & countryName,
                     uint64_t dataPointNum);

  /// \brief Saves csv file with numbers of DataPoints for each mwm to |csvPath|.
  /// If |csvPath| is empty it does nothing.
  void SaveMwmDistributionToCsv(std::string const & csvPath);

  NameToCountMapping const & GetMwmToTotalDataPointsForTesting() const;
  NameToCountMapping const & GetCountryToTotalDataPointsForTesting() const;

private:
  /// \note These fields may present mapping from territory name to either DataPoints
  /// or MatchedTrackPoint count.
  NameToCountMapping m_mwmToTotalDataPoints;
  NameToCountMapping m_countryToTotalDataPoints;
};

/// \brief Saves |mapping| as csv to |ss|.
void MappingToCsv(std::string const & keyName, Stats::NameToCountMapping const & mapping,
                  bool printPercentage, std::basic_ostream<char> & ss);
/// \breif Fills |mapping| according to csv in |ss|. Csv header is skipped.
void MappingFromCsv(std::basic_istream<char> & ss, Stats::NameToCountMapping & mapping);

/// \brief Parses tracks from |logFile| and fills |mwmToTracks|.
void ParseTracks(std::string const & logFile, std::shared_ptr<routing::NumMwmIds> const & numMwmIds,
                 MwmToTracks & mwmToTracks);

std::string DebugPrint(Stats const & s);
}  // namespace track_analyzing
