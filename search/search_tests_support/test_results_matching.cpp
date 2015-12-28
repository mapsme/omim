#include "search/search_tests_support/test_results_matching.hpp"

#include "indexer/feature_decl.hpp"
#include "indexer/index.hpp"

#include "std/sstream.hpp"

namespace search
{
namespace tests_support
{
ExactMatch::ExactMatch(MwmSet::MwmId const & mwmId, shared_ptr<TestFeature> feature)
  : m_mwmId(mwmId), m_feature(feature)
{
}

bool ExactMatch::Matches(FeatureType const & feature) const
{
  if (m_mwmId != feature.GetID().m_mwmId)
    return false;
  return m_feature->Matches(feature);
}

string ExactMatch::ToString() const
{
  ostringstream os;
  os << "ExactMatch [ " << DebugPrint(m_mwmId) << ", " << DebugPrint(*m_feature) << " ]";
  return os.str();
}

AlternativesMatch::AlternativesMatch(initializer_list<shared_ptr<MatchingRule>> rules)
  : m_rules(move(rules))
{
}

bool AlternativesMatch::Matches(FeatureType const & feature) const
{
  for (auto const & rule : m_rules)
  {
    if (rule->Matches(feature))
      return true;
  }
  return false;
}

string AlternativesMatch::ToString() const
{
  ostringstream os;
  os << "OrRule [ ";
  for (auto it = m_rules.cbegin(); it != m_rules.cend(); ++it)
  {
    os << (*it)->ToString();
    if (it + 1 != m_rules.cend())
      os << " | ";
  }
  os << " ]";
  return os.str();
}

bool MatchResults(Index const & index, vector<shared_ptr<MatchingRule>> rules,
                  vector<search::Result> const & actual)
{
  vector<FeatureID> resultIds;
  for (auto const & a : actual)
    resultIds.push_back(a.GetFeatureID());
  sort(resultIds.begin(), resultIds.end());

  vector<string> unexpected;
  auto removeMatched = [&rules, &unexpected](FeatureType const & feature)
  {
    for (auto it = rules.begin(); it != rules.end(); ++it)
    {
      if ((*it)->Matches(feature))
      {
        rules.erase(it);
        return;
      }
    }
    unexpected.push_back(DebugPrint(feature) + " from " + DebugPrint(feature.GetID().m_mwmId));
  };
  index.ReadFeatures(removeMatched, resultIds);

  if (rules.empty() && unexpected.empty())
    return true;

  ostringstream os;
  os << "Unsatisfied rules:" << endl;
  for (auto const & e : rules)
    os << "  " << DebugPrint(*e) << endl;
  os << "Unexpected retrieved features:" << endl;
  for (auto const & u : unexpected)
    os << "  " << u << endl;

  LOG(LWARNING, (os.str()));
  return false;
}

string DebugPrint(MatchingRule const & rule) { return rule.ToString(); }
}  // namespace tests_support
}  // namespace search
