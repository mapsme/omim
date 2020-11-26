#include "testing/testing.hpp"
#include "routing/routing_integration_tests/routing_test_tools.hpp"

#include "generator/borders.hpp"

#include "storage/country_decl.hpp"

#include "geometry/mercator.hpp"

#include "geometry/point2d.hpp"

#include "base/logging.hpp"

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "3party/gflags/src/gflags/gflags.h"

using namespace routing;
using namespace std;
using storage::CountryInfo;

double constexpr kMinimumRouteDistanceM = 10000.0;
double constexpr kRouteLengthAccuracy =  0.15;

// Testing stub to make routing test tools linkable.
static CommandLineOptions g_options;
CommandLineOptions const & GetTestingOptions() {return g_options;}

// @TODO Fix comment
DEFINE_string(mode, "", "Routing consistency tests mode.\n"
              "dist - builds route and compares with distance set in input_file. In that case "
              "every line of input file should contain 5 numbers (doubles) separated by spaces: "
              "<route length in meters> <start lat> <start lon> <finish lat> <finish lon>\n"
              "two_threads - builds route with the help of two threads bidirectional A* and "
              "compares with one thread bidirectional A*. In that case "
              "every line of input file should contain 4 numbers (doubles) separated by spaces: "
              "<start lat> <start lon> <finish lat> <finish lon>");
DEFINE_string(input_file, "", "File with statistics output.");
DEFINE_string(data_path, "../../data/", "Working directory, 'path_to_exe/../../data' if empty.");
DEFINE_string(user_resource_path, "", "User defined resource path for classificator.txt and etc.");
DEFINE_bool(verbose, false, "Output processed lines to log.");
DEFINE_uint64(confidence, 5, "Maximum test count for each single mwm file."
              "Actual for dist mode only.");

// Information about successful user routing.
struct UserRoutingRecord
{
  m2::PointD start;
  m2::PointD stop;
  double distanceM = 0.0;
};

double GetDouble(string const & incomingString, size_t & i)
{
  // Removes leading spaces.
  while (i < incomingString.size() && incomingString[i] == ' ')
  {
    ++i;
  };
  auto end = incomingString.find(" ", i);
  string number = incomingString.substr(i, end - i);
  i = end;
  return stod(number);
}

// Parsing value from statistics.
double GetDouble(string const & incomingString, string const & key)
{
  auto it = incomingString.find(key);
  if (it == string::npos)
    return 0;
  // Skip "key="
  it += key.size() + 1;
  return GetDouble(incomingString, it);
}

// Decoding statistics line. Returns true if the incomeString is the valid record about
// OSRM routing attempt.
bool ParseUserString(string const & incomeString, UserRoutingRecord & result)
{
  // Check if it is a proper routing record.
  if (incomeString.find("Routing_CalculatingRoute") == string::npos)
    return false;
  if (incomeString.find("result=NoError") == string::npos)
    return false;
  if (incomeString.find("name=vehicle") == string::npos)
    return false;
  if (GetDouble(incomeString, "startDirectionX") != 0 && GetDouble(incomeString, "startDirectionY") != 0)
    return false;

  // Extract numbers from a record.
  result.distanceM = GetDouble(incomeString, "distance");
  result.start = mercator::FromLatLon(GetDouble(incomeString, "startLat"), GetDouble(incomeString, "startLon"));
  result.stop = mercator::FromLatLon(GetDouble(incomeString, "finalLat"), GetDouble(incomeString, "finalLon"));
  return true;
}

// Parsing a line with four numbers: <start lat> <start lon> <finish lat> <finish lon>.
void ParseRouteLine(string const & incomeString, UserRoutingRecord & result)
{
  size_t i = 0;
  auto startLat = GetDouble(incomeString, i);
  auto startLon = GetDouble(incomeString, i);
  result.start = mercator::FromLatLon(startLat, startLon);

  auto finishLat = GetDouble(incomeString, i);
  auto finishLon = GetDouble(incomeString, i);
  result.stop = mercator::FromLatLon(finishLat, finishLon);
}

class RouteTester
{
public:
  RouteTester() : m_components(integration::GetVehicleComponents(VehicleType::Car))
  {
  }

  bool BuildRoute(UserRoutingRecord const & record)
  {
    m_components.GetRouter().ClearState();
    auto const result = integration::CalculateRoute(m_components, record.start, m2::PointD::Zero(), record.stop);
    if (result.second != RouterResultCode::NoError)
    {
      LOG(LINFO, ("Can't build the route. Code:", result.second));
      return false;
    }
    auto const delta = record.distanceM * kRouteLengthAccuracy;
    auto const routeLength = result.first->GetTotalDistanceMeters();
    if (abs(routeLength - record.distanceM) < delta)
      return true;

    LOG(LINFO, ("Route has invalid length. Expected:", record.distanceM, "have:", routeLength));
    return false;
  }

  bool CheckRecord(UserRoutingRecord const & record)
  {
    CountryInfo startCountry, finishCountry;
    m_components.GetCountryInfoGetter().GetRegionInfo(record.start, startCountry);
    m_components.GetCountryInfoGetter().GetRegionInfo(record.stop, finishCountry);
    if (startCountry.m_name != finishCountry.m_name || startCountry.m_name.empty())
      return false;

    if (record.distanceM < kMinimumRouteDistanceM)
      return false;

    if (m_checkedCountries[startCountry.m_name] > FLAGS_confidence)
      return false;

    return true;
  }

  bool BuildRouteByRecord(UserRoutingRecord const & record)
  {
    CountryInfo startCountry;
    m_components.GetCountryInfoGetter().GetRegionInfo(record.start, startCountry);
    m_checkedCountries[startCountry.m_name] += 1;
    if (!BuildRoute(record))
    {
      m_errors[startCountry.m_name] += 1;
      return false;
    }
    return true;
  }

  void PrintStatistics()
  {
    LOG(LINFO, ("Checked", m_checkedCountries.size(), "countries."));
    LOG(LINFO, ("Found", m_errors.size(), "maps with errors."));
    for (auto const & record : m_errors)
    {
      if (record.second == m_checkedCountries[record.first])
        LOG(LINFO, ("ERROR!", record.first, " seems to be broken!"));
      else
        LOG(LINFO, ("Warning! Country:", record.first, "has", record.second, "errors on",
                    m_checkedCountries[record.first], "checks"));
    }
  }

private:
  integration::IRouterComponents & m_components;

  map<string, size_t> m_checkedCountries;
  map<string, size_t> m_errors;
};

void ReadInputDist(istream & stream, RouteTester & tester)
{
  string line;
  while (stream.good())
  {
    getline(stream, line);
    strings::Trim(line);
    if (line.empty())
      continue;
    UserRoutingRecord record;
    if (!ParseUserString(line, record))
      continue;
    if (tester.CheckRecord(record))
    {
      if (FLAGS_verbose)
        LOG(LINFO, ("Checked", line));
      tester.BuildRouteByRecord(record);
    }
  }
  tester.PrintStatistics();
}

void ReadInputTwoThreads(istream & stream)
{
  integration::IRouterComponents & components = integration::GetVehicleComponents(VehicleType::Car);
  string line;
  while (stream.good())
  {
    getline(stream, line);
    if (line.empty())
      continue;

    UserRoutingRecord record;
    ParseRouteLine(line, record);
    LOG(LINFO, (record.start, record.stop));

    vector<std::pair<std::shared_ptr<Route>, RouterResultCode>> result;
    for (bool useTwoThreads : {false, true})
    {
      components.GetRouter().ClearState();
      result.push_back(integration::CalculateRoute(
          components, record.start, m2::PointD::Zero(), record.stop, useTwoThreads));
    }
    CHECK_EQUAL(result[0].second, result[1].second, (line));
    if (result[0].second == RouterResultCode::NoError)
    {
//      CHECK_EQUAL(result[0].first->GetPoly().GetSize(), result[1].first->GetPoly().GetSize(),
//                  (line));
//      CHECK_EQUAL(result[0].first->GetTotalDistanceMeters(),
//                  result[1].first->GetTotalDistanceMeters(), (line));
      for (size_t i = 0; i < result[0].first->GetPoly().GetSize(); ++i)
      {
        CHECK_EQUAL(result[0].first->GetPoly().GetPoint(i), result[1].first->GetPoly().GetPoint(i),
                    ("i:", i, line, mercator::ToLatLon(result[0].first->GetPoly().GetPoint(i)), mercator::ToLatLon(result[1].first->GetPoly().GetPoint(i)),
                     "dist one thread:", result[0].first->GetTotalDistanceMeters(), "dist two threads:", result[1].first->GetTotalDistanceMeters()));
      }
    }
  }
}

int main(int argc, char ** argv)
{
  google::SetUsageMessage("Check mwm and routing files consistency. Calculating roads from a user statistics.");

  google::ParseCommandLineFlags(&argc, &argv, true);

  g_options.m_dataPath = FLAGS_data_path.c_str();
  g_options.m_resourcePath = FLAGS_user_resource_path.c_str();
  if (FLAGS_input_file.empty())
    return 1;

  ifstream stream(FLAGS_input_file);
  if (FLAGS_mode == "dist")
  {
    RouteTester tester;
    ReadInputDist(stream, tester);
  }
  else if (FLAGS_mode == "two_threads")
  {
    ReadInputTwoThreads(stream);
  }
  else
  {
    return 1;
  }

  return 0;
}
