#include "search/search_quality/matcher.hpp"

#include "search/feature_loader.hpp"

#include "indexer/feature.hpp"
#include "indexer/feature_algo.hpp"

#include "base/string_utils.hpp"

#include "geometry/mercator.hpp"

#include "base/stl_add.hpp"

namespace search
{
// static
size_t constexpr Matcher::kInvalidId;

Matcher::Matcher(FeatureLoader & loader) : m_loader(loader) {}

void Matcher::Match(std::vector<Sample::Result> const & golden, std::vector<Result> const & actual,
                    std::vector<size_t> & goldenMatching, std::vector<size_t> & actualMatching)
{
  auto const n = golden.size();
  auto const m = actual.size();

  goldenMatching.assign(n, kInvalidId);
  actualMatching.assign(m, kInvalidId);

  // TODO (@y, @m): use Kuhn algorithm here for maximum matching.
  for (size_t i = 0; i < n; ++i)
  {
    if (goldenMatching[i] != kInvalidId)
      continue;
    auto const & g = golden[i];

    for (size_t j = 0; j < m; ++j)
    {
      if (actualMatching[j] != kInvalidId)
        continue;

      auto const & a = actual[j];
      if (Matches(g, a))
      {
        goldenMatching[i] = j;
        actualMatching[j] = i;
        break;
      }
    }
  }
}

bool Matcher::Matches(Sample::Result const & golden, search::Result const & actual)
{
  static double constexpr kToleranceMeters = 50;

  if (actual.GetResultType() != Result::RESULT_FEATURE)
    return false;

  FeatureType ft;
  if (!m_loader.Load(actual.GetFeatureID(), ft))
    return false;

  auto const houseNumber = ft.GetHouseNumber();
  auto const center = feature::GetCenter(ft);

  bool nameMatches = false;
  if (golden.m_name.empty())
  {
    nameMatches = true;
  }
  else
  {
    ft.ForEachName([&golden, &nameMatches](int8_t /* lang */, string const & name) {
      if (golden.m_name == strings::MakeUniString(name))
      {
        nameMatches = true;
        return false;  // breaks the loop
      }
      return true;  // continues the loop
    });
  }

  return nameMatches && golden.m_houseNumber == houseNumber &&
         MercatorBounds::DistanceOnEarth(golden.m_pos, center) < kToleranceMeters;
}
}  // namespace search
