#include "search/search_params.hpp"

#include "indexer/classificator_loader.hpp"
#include "indexer/data_header.hpp"
#include "indexer/data_source.hpp"
#include "indexer/mwm_set.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"

#include "search/ranking_info.hpp"
#include "search/result.hpp"
#include "search/search_quality/helpers.hpp"
#include "search/search_tests_support/test_search_engine.hpp"
#include "search/search_tests_support/test_search_request.hpp"

#include "platform/country_file.hpp"
#include "platform/local_country_file.hpp"
#include "platform/local_country_file_utils.hpp"
#include "platform/platform.hpp"

#include "storage/country_info_getter.hpp"
#include "storage/index.hpp"
#include "storage/storage.hpp"

#include "coding/file_name_utils.hpp"
#include "coding/reader_wrapper.hpp"

#include "base/logging.hpp"
#include "base/scope_guard.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"
#include "base/timer.hpp"

#include "std/algorithm.hpp"
#include "std/cmath.hpp"
#include "std/cstdio.hpp"
#include "std/fstream.hpp"
#include "std/iomanip.hpp"
#include "std/iostream.hpp"
#include "std/limits.hpp"
#include "std/map.hpp"
#include "std/numeric.hpp"
#include "std/sstream.hpp"
#include "std/string.hpp"
#include "std/vector.hpp"

#include "defines.hpp"

#include "3party/gflags/src/gflags/gflags.h"

using namespace search::tests_support;
using namespace search;
using namespace storage;

DEFINE_string(data_path, "", "Path to data directory (resources dir)");
DEFINE_string(locale, "en", "Locale of all the search queries");
DEFINE_int32(num_threads, 1, "Number of search engine threads");
DEFINE_string(mwm_list_path, "",
              "Path to a file containing the names of available mwms, one per line");
DEFINE_string(mwm_path, "", "Path to mwm files (writable dir)");
DEFINE_string(queries_path, "", "Path to the file with queries");
DEFINE_int32(top, 1, "Number of top results to show for every query");
DEFINE_string(viewport, "", "Viewport to use when searching (default, moscow, london, zurich)");
DEFINE_string(check_completeness, "", "Path to the file with completeness data");
DEFINE_string(ranking_csv_file, "", "File ranking info will be exported to");

map<string, m2::RectD> const kViewports = {
    {"default", m2::RectD(m2::PointD(0.0, 0.0), m2::PointD(1.0, 1.0))},
    {"moscow", MercatorBounds::RectByCenterLatLonAndSizeInMeters(55.7, 37.7, 5000)},
    {"london", MercatorBounds::RectByCenterLatLonAndSizeInMeters(51.5, 0.0, 5000)},
    {"zurich", MercatorBounds::RectByCenterLatLonAndSizeInMeters(47.4, 8.5, 5000)}};

string const kDefaultQueriesPathSuffix = "/../search/search_quality/search_quality_tool/queries.txt";
string const kEmptyResult = "<empty>";

struct CompletenessQuery
{
  string m_query;
  unique_ptr<TestSearchRequest> m_request;
  string m_mwmName;
  uint32_t m_featureId = 0;
  double m_lat = 0;
  double m_lon = 0;
};

DECLARE_EXCEPTION(MalformedQueryException, RootException);

void DidDownload(TCountryId const & /* countryId */,
                 shared_ptr<platform::LocalCountryFile> const & /* localFile */)
{
}

bool WillDelete(TCountryId const & /* countryId */,
                shared_ptr<platform::LocalCountryFile> const & /* localFile */)
{
  return false;
}

string MakePrefixFree(string const & query) { return query + " "; }

void ReadStringsFromFile(string const & path, vector<string> & result)
{
  ifstream stream(path.c_str());
  CHECK(stream.is_open(), ("Can't open", path));

  string s;
  while (getline(stream, s))
  {
    strings::Trim(s);
    if (!s.empty())
      result.emplace_back(s);
  }
}

// If n == 1, prints the query and the top result separated by a tab.
// Otherwise, prints the query on a separate line
// and then prints n top results on n lines starting with tabs.
void PrintTopResults(string const & query, vector<Result> const & results, size_t n,
                     double elapsedSeconds)
{
  cout << query;
  char timeBuf[100];
  snprintf(timeBuf, sizeof(timeBuf), "\t[%.3fs]", elapsedSeconds);
  if (n > 1)
    cout << timeBuf;
  for (size_t i = 0; i < n; ++i)
  {
    if (n > 1)
      cout << endl;
    cout << "\t";
    if (i < results.size())
      // todo(@m) Print more information: coordinates, viewport, etc.
      cout << results[i].GetString();
    else
      cout << kEmptyResult;
  }
  if (n == 1)
    cout << timeBuf;
  cout << endl;
}

uint64_t ReadVersionFromHeader(platform::LocalCountryFile const & mwm)
{
  vector<string> specialFiles = {
    WORLD_FILE_NAME,
    WORLD_COASTS_FILE_NAME,
    WORLD_COASTS_OBSOLETE_FILE_NAME
  };
  for (auto const & name : specialFiles)
  {
    if (mwm.GetCountryName() == name)
      return mwm.GetVersion();
  }

  ModelReaderPtr reader = FilesContainerR(mwm.GetPath(MapOptions::Map)).GetReader(VERSION_FILE_TAG);
  ReaderSrc src(reader.GetPtr());

  version::MwmVersion version;
  version::ReadVersion(src, version);
  return version.GetVersion();
}

void CalcStatistics(vector<double> const & a, double & avg, double & maximum, double & var,
                    double & stdDev)
{
  avg = 0;
  maximum = 0;
  var = 0;
  stdDev = 0;

  for (auto const x : a)
  {
    avg += static_cast<double>(x);
    maximum = max(maximum, static_cast<double>(x));
  }

  double n = static_cast<double>(a.size());
  if (a.size() > 0)
    avg /= n;

  for (auto const x : a)
    var += pow(static_cast<double>(x) - avg, 2);
  if (a.size() > 1)
    var /= n - 1;
  stdDev = sqrt(var);
}

// Unlike strings::Tokenize, this function allows for empty tokens.
void Split(string const & s, char delim, vector<string> & parts)
{
  istringstream iss(s);
  string part;
  while (getline(iss, part, delim))
    parts.push_back(part);
}

// Returns the position of the result that is expected to be found by geocoder completeness
// tests in the |result| vector or -1 if it does not occur there.
int FindResult(DataSource & dataSource, string const & mwmName, uint32_t const featureId,
               double const lat, double const lon, vector<Result> const & results)
{
  CHECK_LESS_OR_EQUAL(results.size(), numeric_limits<int>::max(), ());
  auto const mwmId = dataSource.GetMwmIdByCountryFile(platform::CountryFile(mwmName));
  FeatureID const expectedFeatureId(mwmId, featureId);
  for (size_t i = 0; i < results.size(); ++i)
  {
    auto const & r = results[i];
    if (r.GetFeatureID() == expectedFeatureId)
      return static_cast<int>(i);
  }

  // Another attempt. If the queries are stale, feature id is useless.
  // However, some information may be recovered from (lat, lon).
  double const kEps = 1e-2;
  for (size_t i = 0; i < results.size(); ++i)
  {
    auto const & r = results[i];
    if (r.HasPoint() &&
        my::AlmostEqualAbs(r.GetFeatureCenter(), MercatorBounds::FromLatLon(lat, lon), kEps))
    {
      double const dist = MercatorBounds::DistanceOnEarth(r.GetFeatureCenter(),
                                                          MercatorBounds::FromLatLon(lat, lon));
      LOG(LDEBUG, ("dist =", dist));
      return static_cast<int>(i);
    }
  }
  return -1;
}

void ParseCompletenessQuery(string & s, CompletenessQuery & q)
{
  s.append(" ");

  vector<string> parts;
  Split(s, ';', parts);
  if (parts.size() != 7)
  {
    MYTHROW(MalformedQueryException,
            ("Can't split", s, ", found", parts.size(), "part(s):", parts));
  }

  auto idx = parts[0].find(':');
  if (idx == string::npos)
    MYTHROW(MalformedQueryException, ("Could not find \':\':", s));

  string mwmName = parts[0].substr(0, idx);
  string const kMwmSuffix = ".mwm";
  if (!strings::EndsWith(mwmName, kMwmSuffix))
    MYTHROW(MalformedQueryException, ("Bad mwm name:", s));

  string const featureIdStr = parts[0].substr(idx + 1);
  uint64_t featureId;
  if (!strings::to_uint64(featureIdStr, featureId))
    MYTHROW(MalformedQueryException, ("Bad feature id:", s));

  string const type = parts[1];
  double lon, lat;
  if (!strings::to_double(parts[2].c_str(), lon) || !strings::to_double(parts[3].c_str(), lat))
    MYTHROW(MalformedQueryException, ("Bad lon-lat:", s));

  string const city = parts[4];
  string const street = parts[5];
  string const house = parts[6];

  mwmName = mwmName.substr(0, mwmName.size() - kMwmSuffix.size());
  string country = mwmName;
  replace(country.begin(), country.end(), '_', ' ');

  q.m_query = country + " " + city + " " + street + " " + house + " ";
  q.m_mwmName = mwmName;
  q.m_featureId = static_cast<uint32_t>(featureId);
  q.m_lat = lat;
  q.m_lon = lon;
}

// Reads queries in the format
//   CountryName.mwm:featureId;type;lon;lat;city;street;<housenumber or housename>
// from |path|, executes them against the |engine| with viewport set to |viewport|
// and reports the number of queries whose expected result is among the returned results.
// Exact feature id is expected, but a close enough (lat, lon) is good too.
void CheckCompleteness(string const & path, m2::RectD const & viewport, DataSource & dataSource,
                       TestSearchEngine & engine)
{
  my::ScopedLogAbortLevelChanger const logAbortLevel(LCRITICAL);

  ifstream stream(path.c_str());
  CHECK(stream.is_open(), ("Can't open", path));

  my::Timer timer;

  uint32_t totalQueries = 0;
  uint32_t malformedQueries = 0;
  uint32_t expectedResultsFound = 0;
  uint32_t expectedResultsTop1 = 0;

  // todo(@m) Process the queries on the fly and do not keep them.
  vector<CompletenessQuery> queries;

  string s;
  while (getline(stream, s))
  {
    ++totalQueries;
    try
    {
      CompletenessQuery q;
      ParseCompletenessQuery(s, q);
      q.m_request = make_unique<TestSearchRequest>(engine, q.m_query, FLAGS_locale,
                                                   Mode::Everywhere, viewport);
      queries.push_back(move(q));
    }
    catch (MalformedQueryException e)
    {
      LOG(LERROR, (e.what()));
      ++malformedQueries;
    }
  }

  for (auto & q : queries)
  {
    q.m_request->Run();

    LOG(LDEBUG, (q.m_query, q.m_request->Results()));
    int pos =
        FindResult(dataSource, q.m_mwmName, q.m_featureId, q.m_lat, q.m_lon, q.m_request->Results());
    if (pos >= 0)
      ++expectedResultsFound;
    if (pos == 0)
      ++expectedResultsTop1;
  }

  double const expectedResultsFoundPercentage =
      totalQueries == 0 ? 0 : 100.0 * static_cast<double>(expectedResultsFound) /
                                  static_cast<double>(totalQueries);
  double const expectedResultsTop1Percentage =
      totalQueries == 0 ? 0 : 100.0 * static_cast<double>(expectedResultsTop1) /
                                  static_cast<double>(totalQueries);

  cout << "Time spent on checking completeness: " << timer.ElapsedSeconds() << "s." << endl;
  cout << "Total queries: " << totalQueries << endl;
  cout << "Malformed queries: " << malformedQueries << endl;
  cout << "Expected results found: " << expectedResultsFound << " ("
       << expectedResultsFoundPercentage << "%)." << endl;
  cout << "Expected results found in the top1 slot: " << expectedResultsTop1 << " ("
       << expectedResultsTop1Percentage << "%)." << endl;
}

int main(int argc, char * argv[])
{
  ChangeMaxNumberOfOpenFiles(kMaxOpenFiles);

  ios_base::sync_with_stdio(false);
  Platform & platform = GetPlatform();

  google::SetUsageMessage("Search quality tests.");
  google::ParseCommandLineFlags(&argc, &argv, true);

  string countriesFile = COUNTRIES_FILE;
  if (!FLAGS_data_path.empty())
  {
    platform.SetResourceDir(FLAGS_data_path);
    countriesFile = my::JoinFoldersToPath(FLAGS_data_path, COUNTRIES_FILE);
  }

  if (!FLAGS_mwm_path.empty())
    platform.SetWritableDirForTests(FLAGS_mwm_path);

  LOG(LINFO, ("writable dir =", platform.WritableDir()));
  LOG(LINFO, ("resources dir =", platform.ResourcesDir()));

  Storage storage(countriesFile);
  storage.Init(&DidDownload, &WillDelete);
  auto infoGetter = CountryInfoReader::CreateCountryInfoReader(platform);
  infoGetter->InitAffiliationsInfo(&storage.GetAffiliations());

  classificator::Load();

  Engine::Params params;
  params.m_locale = FLAGS_locale;
  params.m_numThreads = FLAGS_num_threads;

  FrozenDataSource dataSource;

  vector<platform::LocalCountryFile> mwms;
  if (!FLAGS_mwm_list_path.empty())
  {
    vector<string> availableMwms;
    ReadStringsFromFile(FLAGS_mwm_list_path, availableMwms);
    for (auto const & countryName : availableMwms)
      mwms.emplace_back(platform.WritableDir(), platform::CountryFile(countryName), 0);
  }
  else
  {
    platform::FindAllLocalMapsAndCleanup(numeric_limits<int64_t>::max() /* the latest version */,
                                         mwms);
    for (auto & map : mwms)
      map.SyncWithDisk();
  }
  cout << "Mwms used in all search invocations:" << endl;
  for (auto & mwm : mwms)
  {
    mwm.SyncWithDisk();
    cout << mwm.GetCountryName() << " " << ReadVersionFromHeader(mwm) << endl;
    dataSource.RegisterMap(mwm);
  }
  cout << endl;

  TestSearchEngine engine(dataSource, move(infoGetter), params);

  m2::RectD viewport;
  {
    string name = FLAGS_viewport;
    auto it = kViewports.find(name);
    if (it == kViewports.end())
    {
      name = "default";
      it = kViewports.find(name);
    }
    CHECK(it != kViewports.end(), ());
    viewport = it->second;
    cout << "Viewport used in all search invocations: " << name << " " << DebugPrint(viewport)
         << endl
         << endl;
  }

  if (!FLAGS_check_completeness.empty())
  {
    CheckCompleteness(FLAGS_check_completeness, viewport, dataSource, engine);
    return 0;
  }

  vector<string> queries;
  string queriesPath = FLAGS_queries_path;
  if (queriesPath.empty())
    queriesPath = my::JoinFoldersToPath(platform.WritableDir(), kDefaultQueriesPathSuffix);
  ReadStringsFromFile(queriesPath, queries);

  vector<unique_ptr<TestSearchRequest>> requests;
  for (size_t i = 0; i < queries.size(); ++i)
  {
    // todo(@m) Add a bool flag to search with prefixes?
    requests.emplace_back(make_unique<TestSearchRequest>(engine, MakePrefixFree(queries[i]),
                                                         FLAGS_locale, Mode::Everywhere, viewport));
  }

  ofstream csv;
  bool dumpCSV = false;
  if (!FLAGS_ranking_csv_file.empty())
  {
    csv.open(FLAGS_ranking_csv_file);
    if (!csv.is_open())
      LOG(LERROR, ("Can't open file for CSV dump:", FLAGS_ranking_csv_file));
    else
      dumpCSV = true;
  }

  if (dumpCSV)
  {
    RankingInfo::PrintCSVHeader(csv);
    csv << endl;
  }

  vector<double> responseTimes(queries.size());
  for (size_t i = 0; i < queries.size(); ++i)
  {
    requests[i]->Run();
    auto rt = duration_cast<milliseconds>(requests[i]->ResponseTime()).count();
    responseTimes[i] = static_cast<double>(rt) / 1000;
    PrintTopResults(MakePrefixFree(queries[i]), requests[i]->Results(), FLAGS_top,
                    responseTimes[i]);

    if (dumpCSV)
    {
      for (auto const & result : requests[i]->Results())
      {
        result.GetRankingInfo().ToCSV(csv);
        csv << endl;
      }
    }
  }

  double averageTime;
  double maxTime;
  double varianceTime;
  double stdDevTime;
  CalcStatistics(responseTimes, averageTime, maxTime, varianceTime, stdDevTime);

  cout << fixed << setprecision(3);
  cout << endl;
  cout << "Maximum response time: " << maxTime << "s" << endl;
  cout << "Average response time: " << averageTime << "s"
       << " (std. dev. " << stdDevTime << "s)" << endl;

  return 0;
}
