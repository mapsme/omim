#include "testing/testing.hpp"

#include "search/countries_names_index.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace search;
using namespace storage;
using namespace std;

namespace
{
UNIT_TEST(CountriesNamesIndex_Smoke)
{
  search::CountriesNamesIndex index;

  {
    string const query = "";
    vector<CountryId> results;
    index.CollectMatchingCountries(query, results);
    TEST(results.empty(), ());
  }

  {
    string const query = "Чехия ";
    vector<CountryId> results;
    index.CollectMatchingCountries(query, results);
    TEST_EQUAL(results.size(), 2, (results));
    TEST_EQUAL(results[0], "Czech Republic", ());
    TEST_EQUAL(results[1], "Czech Republic Short", ());
  }

  {
    string const query = "Slovenia";
    vector<CountryId> results;
    index.CollectMatchingCountries(query, results);
    TEST_EQUAL(results.size(), 4, ());
    sort(results.begin(), results.end());
    TEST_EQUAL(results[0], "Slovakia", ());
    TEST_EQUAL(results[1], "Slovenia", ());
    TEST_EQUAL(results[2], "Slovenia_East", ());
    TEST_EQUAL(results[3], "Slovenia_West", ());
  }
}
}  // namespace
