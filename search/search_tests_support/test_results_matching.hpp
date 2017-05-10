#pragma once

#include "search/result.hpp"

#include "indexer/mwm_set.hpp"

#include <memory>
#include <string>
#include <vector>

class FeatureType;
class Index;

namespace generator
{
namespace tests_support
{
class TestFeature;
}
}

namespace search
{
namespace tests_support
{
class MatchingRule
{
public:
  virtual ~MatchingRule() = default;

  virtual bool Matches(FeatureType const & feature) const = 0;
  virtual std::string ToString() const = 0;
};

class ExactMatchingRule : public MatchingRule
{
public:
  ExactMatchingRule(MwmSet::MwmId const & mwmId,
                    generator::tests_support::TestFeature const & feature);

  // MatchingRule overrides:
  bool Matches(FeatureType const & feature) const override;
  std::string ToString() const override;

private:
  MwmSet::MwmId m_mwmId;
  generator::tests_support::TestFeature const & m_feature;
};

class AlternativesMatchingRule : public MatchingRule
{
public:
  AlternativesMatchingRule(initializer_list<std::shared_ptr<MatchingRule>> rules);

  // MatchingRule overrides:
  bool Matches(FeatureType const & feature) const override;
  std::string ToString() const override;

private:
  std::vector<std::shared_ptr<MatchingRule>> m_rules;
};

template <typename... TArgs>
shared_ptr<MatchingRule> ExactMatch(TArgs &&... args)
{
  return std::make_shared<ExactMatchingRule>(forward<TArgs>(args)...);
}

template <typename... TArgs>
shared_ptr<MatchingRule> AlternativesMatch(TArgs &&... args)
{
  return std::make_shared<AlternativesMatchingRule>(forward<TArgs>(args)...);
}

bool MatchResults(Index const & index, std::vector<std::shared_ptr<MatchingRule>> rules,
                  std::vector<search::Result> const & actual);

std::string DebugPrint(MatchingRule const & rule);
}  // namespace tests_support
}  // namespace search
