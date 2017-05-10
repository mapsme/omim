#include "search/search_tests_support/test_results_matching.hpp"

#include "generator/generator_tests_support/test_feature.hpp"

#include "indexer/feature_decl.hpp"
#include "indexer/index.hpp"

#include <sstream>

using namespace generator::tests_support;

namespace search
{
namespace tests_support
{
ExactMatchingRule::ExactMatchingRule(MwmSet::MwmId const & mwmId, TestFeature const & feature)
  : m_mwmId(mwmId), m_feature(feature)
{
}

bool ExactMatchingRule::Matches(FeatureType const & feature) const
{
  if (m_mwmId != feature.GetID().m_mwmId)
    return false;
  return m_feature.Matches(feature);
}

std::string ExactMatchingRule::ToString() const
{
  std::ostringstream os;
  os << "ExactMatchingRule [ " << DebugPrint(m_mwmId) << ", " << DebugPrint(m_feature) << " ]";
  return os.str();
}

AlternativesMatchingRule::AlternativesMatchingRule(initializer_list<std::shared_ptr<MatchingRule>> rules)
  : m_rules(move(rules))
{
}

bool AlternativesMatchingRule::Matches(FeatureType const & feature) const
{
  for (auto const & rule : m_rules)
  {
    if (rule->Matches(feature))
      return true;
  }
  return false;
}

std::string AlternativesMatchingRule::ToString() const
{
  std::ostringstream os;
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

bool MatchResults(Index const & index, std::vector<std::shared_ptr<MatchingRule>> rules,
                  std::vector<search::Result> const & actual)
{
  std::vector<FeatureID> resultIds;
  for (auto const & a : actual)
    resultIds.push_back(a.GetFeatureID());
  sort(resultIds.begin(), resultIds.end());

  std::vector<std::string> unexpected;
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

  std::ostringstream os;
  os << "Unsatisfied rules:" << endl;
  for (auto const & e : rules)
    os << "  " << DebugPrint(*e) << endl;
  os << "Unexpected retrieved features:" << endl;
  for (auto const & u : unexpected)
    os << "  " << u << endl;

  LOG(LWARNING, (os.str()));
  return false;
}

std::string DebugPrint(MatchingRule const & rule) { return rule.ToString(); }
}  // namespace tests_support
}  // namespace search
