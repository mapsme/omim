#include "track_analyzing/exceptions.hpp"
#include "track_analyzing/track.hpp"

#include "indexer/classificator.hpp"
#include "indexer/classificator_loader.hpp"

#include "3party/gflags/src/gflags/gflags.h"

using namespace std;
using namespace tracking;

namespace
{
#define DEFINE_string_ext(name, value, description)                           \
  DEFINE_string(name, value, description);                                    \
                                                                              \
  string const & Demand_##name()                                              \
  {                                                                           \
    if (FLAGS_##name.empty())                                                 \
      MYTHROW(MessageException, (string("Specify the argument --") + #name)); \
                                                                              \
    return FLAGS_##name;                                                      \
  }

DEFINE_string_ext(cmd, "", "command: match, info, cpptrack");
DEFINE_string_ext(in, "", "input log file name");
DEFINE_string(out, "", "output track file name");
DEFINE_string_ext(mwm, "", "short mwm name");
DEFINE_string_ext(user, "", "user id");
DEFINE_int32(track, -1, "track index");

DEFINE_string(track_extension, ".track", "track files extension");
DEFINE_bool(no_world_logs, false, "don't print world summary logs");
DEFINE_bool(no_mwm_logs, false, "don't print logs per mwm");
DEFINE_bool(no_track_logs, false, "don't print logs per track");

DEFINE_uint64(min_duration, 5 * 60, "minimal track duration in seconds");
DEFINE_double(min_length, 1000.0, "minimal track length in meters");
DEFINE_double(min_speed, 15.0, "minimal track average speed in km/hour");
DEFINE_double(max_speed, 110.0, "maximum track average speed in km/hour");
DEFINE_bool(ignore_traffic, true, "ignore tracks with traffic data");

size_t Demand_track()
{
  if (FLAGS_track < 0)
    MYTHROW(MessageException, ("Specify the --track key"));

  return static_cast<size_t>(FLAGS_track);
}
}  // namespace

namespace tracking
{
void CmdCppTrack(string const & trackFile, string const & mwmName, string const & user,
                 size_t trackIdx);
void CmdMatch(string const & logFile, string const & trackFile);
void CmdTagsTable(string const & filepath, string const & trackExtension, string const & mwmFilter,
                  string const & userFilter);
void CmdTrack(string const & trackFile, string const & mwmName, string const & user,
              size_t trackIdx);
void CmdTracks(string const & filepath, string const & trackExtension, string const & mwmFilter,
               string const & userFilter, TrackFilter const & filter, bool noTrackLogs,
               bool noMwmLogs, bool noWorldLogs);
}  // namespace tracking

int main(int argc, char ** argv)
{
  google::ParseCommandLineFlags(&argc, &argv, true);
  string const & cmd = Demand_cmd();

  classificator::Load();
  classif().SortClassificator();

  try
  {
    if (cmd == "match")
    {
      string const & logFile = Demand_in();
      CmdMatch(logFile, FLAGS_out.empty() ? logFile + ".track" : FLAGS_out);
    }
    else if (cmd == "tracks")
    {
      TrackFilter const filter(FLAGS_min_duration, FLAGS_min_length, FLAGS_min_speed, FLAGS_max_speed,
                             FLAGS_ignore_traffic);
      CmdTracks(Demand_in(), FLAGS_track_extension, FLAGS_mwm, FLAGS_user, filter,
                FLAGS_no_track_logs, FLAGS_no_mwm_logs, FLAGS_no_world_logs);
    }
    else if (cmd == "track")
    {
      CmdTrack(Demand_in(), Demand_mwm(), Demand_user(), Demand_track());
    }
    else if (cmd == "cpptrack")
    {
      CmdCppTrack(Demand_in(), Demand_mwm(), Demand_user(), Demand_track());
    }
    else if (cmd == "table")
    {
      CmdTagsTable(Demand_in(), FLAGS_track_extension, FLAGS_mwm, FLAGS_user);
    }
    else
    {
      LOG(LWARNING, ("Unknown command", FLAGS_cmd));
      return 1;
    }
  }
  catch (MessageException const & e)
  {
    LOG(LWARNING, (e.Msg()));
    return 1;
  }
  catch (RootException const & e)
  {
    LOG(LERROR, (e.what()));
    return 1;
  }

  return 0;
}
