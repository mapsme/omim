#include "search/countries_names_index.hpp"

#include "platform/platform.hpp"

#include "coding/file_reader.hpp"

#include "base/assert.hpp"

#include <fstream>
#include <set>
#include <sstream>

using namespace std;

namespace search
{
CountriesNamesIndex::CountriesNamesIndex()
{
  ReadCountryNamesFromFile(m_countries);
  BuildIndexFromTranslations();
}

void CountriesNamesIndex::CollectMatchingCountries(string const & query,
                                                   vector<storage::CountryId> & results)
{
  set<size_t> ids;
  auto insertId = [&ids](size_t id, bool /* exactMatch */) { ids.insert(id); };

  vector<strings::UniString> tokens;
  search::NormalizeAndTokenizeString(query, tokens);
  search::Delimiters delims;
  bool const lastTokenIsPrefix = !query.empty() && !delims(strings::LastUniChar(query));
  for (size_t i = 0; i < tokens.size(); ++i)
  {
    auto const & token = tokens[i];
    if (i + 1 == tokens.size() && lastTokenIsPrefix)
      Retrieve<strings::PrefixDFAModifier<strings::LevenshteinDFA>>(token, insertId);
    else
      Retrieve<strings::LevenshteinDFA>(token, insertId);
  }

  // todo(@m) Do not bother with tf/idf for now.
  results.clear();
  for (auto id : ids)
  {
    CHECK_LESS(id, m_countries.size(), ());
    results.emplace_back(m_countries[id].m_countryId);
  }
}

void CountriesNamesIndex::ReadCountryNamesFromFile(vector<Country> & countries)
{
  string contents;

  GetPlatform().GetReader(COUNTRIES_NAMES_FILE)->ReadAsString(contents);
  istringstream ifs(contents);

  string line;
  countries.clear();
  while (getline(ifs, line))
  {
    if (line.empty())
      continue;
    strings::Trim(line);
    if (line[0] == '[')
    {
      CHECK_EQUAL(line[line.size() - 1], ']', ());
      countries.push_back({});
      countries.back().m_countryId = line.substr(1, line.size() - 2);
      continue;
    }
    auto pos = line.find('=');
    if (pos == string::npos)
      continue;
    // Ignore the language code: the language sets differ for StringUtf8Multilang
    // and for the translations used by this class.
    auto t = line.substr(pos + 1);
    strings::Trim(t);
    if (!countries.empty())
      countries.back().m_doc.m_translations.push_back(t);
  }
}

void CountriesNamesIndex::BuildIndexFromTranslations()
{
  for (size_t i = 0; i < m_countries.size(); ++i)
    m_index.Add(i, m_countries[i].m_doc);
}
}  // namespace search
