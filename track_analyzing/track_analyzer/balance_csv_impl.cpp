#include "track_analyzing/track_analyzer/balance_csv_impl.hpp"

#include "track_analyzing/track_analyzer/utils.hpp"

#include "coding/csv_reader.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/math.hpp"

#include <algorithm>
#include <random>
#include <utility>

namespace
{
double constexpr kSumFractionEps = 1e-5;
}  // namespace

namespace track_analyzing
{
using namespace std;

void FillTable(basic_istream<char> & tableCsvStream, MwmToDataPoints & matchedDataPoints,
               vector<TableRow> & table)
{
  size_t constexpr kTableColumns = 19;
  for (auto const & row : coding::CSVRunner(
      coding::CSVReader(tableCsvStream, true /* hasHeader */, ',' /* delimiter */)))
  {
    CHECK_EQUAL(row.size(), kTableColumns, (row));
    auto const & mwmName = row[kMwmNameCsvColumn];
    matchedDataPoints[mwmName] += 1;
    table.push_back(row);
  }
}

void RemoveKeysSmallValue(MwmToDataPoints & checkedMap, MwmToDataPoints & additionalMap,
                          uint64_t ignoreDataPointNumber)
{
  CHECK(AreKeysEqual(additionalMap, checkedMap),
        ("Mwms in |checkedMap| and in |additionalMap| should have the same set of keys."));

  for (auto it = checkedMap.begin() ; it != checkedMap.end();)
  {
    auto const dataPointNumberAfterMatching = it->second;
    if (dataPointNumberAfterMatching > ignoreDataPointNumber)
    {
      ++it;
      continue;
    }

    auto const & mwmName = it->first;
    additionalMap.erase(mwmName);
    it = checkedMap.erase(it);
  }
}

MwmToDataPointFraction GetMwmToDataPointFraction(MwmToDataPoints const & numberMapping)
{
  CHECK(!numberMapping.empty(), ());
  uint64_t const totalDataPointNumber = ValueSum(numberMapping);

  MwmToDataPointFraction fractionMapping;
  for (auto const & kv : numberMapping)
    fractionMapping.emplace(kv.first, static_cast<double>(kv.second) / totalDataPointNumber);

  return fractionMapping;
}

MwmToDataPoints CalcsMatchedDataPointsToKeepDistribution(
    MwmToDataPoints const & matchedDataPoints, MwmToDataPointFraction const & distributionFractions)
{
  CHECK(!matchedDataPoints.empty(), ());
  CHECK(AreKeysEqual(matchedDataPoints, distributionFractions),
        ("Mwms in |matchedDataPoints| and in |distributionFractions| should have the same set of keys."));
  CHECK(base::AlmostEqualAbs(ValueSum(distributionFractions), 1.0, kSumFractionEps),
        (ValueSum(distributionFractions)));

  auto const matchedFractions = GetMwmToDataPointFraction(matchedDataPoints);

  double maxRatio = 0.0;
  double maxRationDistributionFraction = 0.0;
  uint64_t maxRationMatchedDataPointNumber = 0;
  // First, let's find such mwm that all |matchedDataPoints| of it may be used and
  // the distribution set in |distributionFractions| will be kept. It's an mwm
  // on which the maximum of ratio
  // (percent of data points in the distribution) / (percent of matched data points)
  // is reached.
  for (auto const & kv : matchedDataPoints)
  {
    auto const & mwmName = kv.first;
    auto const matchedDataPointNumber = kv.second;
    auto const itDistr = distributionFractions.find(mwmName);
    CHECK(itDistr != distributionFractions.cend(), (mwmName));
    auto const distributionFraction = itDistr->second;
    auto const itMatched = matchedFractions.find(mwmName);
    CHECK(itMatched != matchedFractions.cend(), (mwmName));
    double const matchedFraction = itMatched->second;

    double const ratio = distributionFraction / matchedFraction;
    if (ratio > maxRatio)
    {
      maxRatio = ratio;
      maxRationDistributionFraction = distributionFraction;
      maxRationMatchedDataPointNumber = matchedDataPointNumber;
    }
  }
  CHECK_GREATER_OR_EQUAL(maxRatio, 1.0, ());

  // Having data points number in mwm on which the maximum is reached and
  // fraction of data point in the distribution for this mwm (less or equal 1.0) it's possible to
  // calculate the total matched points number which may be used to keep the distribution.
  auto const totalMatchedPointNumberToKeepDistribution =
      static_cast<uint64_t>(maxRationMatchedDataPointNumber / maxRationDistributionFraction);
  CHECK_LESS_OR_EQUAL(totalMatchedPointNumberToKeepDistribution, ValueSum(matchedDataPoints), ());

  // Having total maximum matched point number which let to keep the distribution
  // and which fraction for each mwm should be used to correspond to the distribution
  // it's possible to find matched data point number which may be used for each mwm
  // to fulfil the distribution.
  MwmToDataPoints matchedDataPointsToKeepDistribution;
  for (auto const & kv : distributionFractions)
  {
    auto const & mwm = kv.first;
    auto const fraction = kv.second;
    auto const matchedDataPointsToKeepDistributionForMwm =
        static_cast<uint64_t>(fraction * totalMatchedPointNumberToKeepDistribution);
    matchedDataPointsToKeepDistribution.emplace(mwm, matchedDataPointsToKeepDistributionForMwm);

    if (matchedDataPointsToKeepDistributionForMwm == 0)
    {
      LOG(LWARNING, ("Zero points should be put to", mwm,
                     "to keep the distribution. Distribution fraction:", fraction,
                     "totalMatchedPointNumberToKeepDistribution:",
                     totalMatchedPointNumberToKeepDistribution));
    }
  }
  return matchedDataPointsToKeepDistribution;
}

MwmToDataPoints BalancedDataPointNumber(MwmToDataPoints && distribution,
                                        MwmToDataPoints && matchedDataPoints,
                                        uint64_t ignoreDataPointsNumber)
{
  // Removing every mwm from |distribution| and |matchedDataPoints| if it has
  // |ignoreDataPointsNumber| data points or less in |matchedDataPoints|.
  RemoveKeysSmallValue(matchedDataPoints, distribution, ignoreDataPointsNumber);

  // Calculating how much percent data points in each mwm of |distribution|.
  auto const distributionFraction = GetMwmToDataPointFraction(distribution);

  CHECK(AreKeysEqual(distribution, matchedDataPoints),
        ("Mwms in |checkedMap| and in |additionalMap| should have the same set of keys."));
  // The distribution defined in |distribution| and the distribution after matching
  // may be different. (They are most likely different.) So to keep the distribution
  // defined in |distribution| after matching only a part of matched data points
  // should be used.
  // Returning mapping from mwm to balanced data point number with information about
  // how many data points form |distributionCsvStream| should be used for every mwm.
  return CalcsMatchedDataPointsToKeepDistribution(matchedDataPoints, distributionFraction);
}

void FilterTable(MwmToDataPoints const & balancedDataPointNumbers, vector<TableRow> & table)
{
  map<string, vector<size_t>> tableIdx;
  for (size_t idx = 0; idx < table.size(); ++idx)
  {
    auto const & row = table[idx];
    tableIdx[row[kMwmNameCsvColumn]].push_back(idx);
  }

  // Shuffling data point indexes and reducing the size of index vector for each mwm according to
  // |balancedDataPointNumbers|.
  random_device rd;
  mt19937 g(rd());
  for (auto & kv : tableIdx)
  {
    auto const & mwmName = kv.first;
    auto & mwmRecordsIndices = kv.second;
    auto it = balancedDataPointNumbers.find(mwmName);
    if (it == balancedDataPointNumbers.end())
    {
      mwmRecordsIndices.resize(0); // No indices.
      continue;
    }

    auto const balancedDataPointNumber = it->second;
    CHECK_LESS_OR_EQUAL(balancedDataPointNumber, mwmRecordsIndices.size(), (mwmName));
    shuffle(mwmRecordsIndices.begin(), mwmRecordsIndices.end(), g);
    mwmRecordsIndices.resize(balancedDataPointNumber);
  }

  // Refilling |table| with part of its items to corresponds |balancedDataPointNumbers| distribution.
  vector<TableRow> balancedTable;
  for (auto const & kv : tableIdx)
  {
    for (auto const idx : kv.second)
      balancedTable.emplace_back(move(table[idx]));
  }
  table = move(balancedTable);
}

void BalanceDataPoints(basic_istream<char> & distributionCsvStream,
                       basic_istream<char> & tableCsvStream, uint64_t ignoreDataPointsNumber,
                       vector<TableRow> & balancedTable)
{
  LOG(LINFO, ("Balancing data points..."));
  // Filling a map mwm to DataPoints number according to distribution csv file.
  MwmToDataPoints distribution;
  MappingFromCsv(distributionCsvStream, distribution);
  uint64_t const totalDistributionDataPointsNumber = ValueSum(distribution);

  balancedTable.clear();
  MwmToDataPoints matchedDataPoints;
  FillTable(tableCsvStream, matchedDataPoints, balancedTable);
  uint64_t const totalMatchedDataPointsNumber = ValueSum(matchedDataPoints);

  if (matchedDataPoints.empty())
  {
    CHECK(balancedTable.empty(), ());
    return;
  }

  // Removing every mwm from |distribution| if there is no such mwm in |matchedDataPoints|.
  for (auto it = distribution.begin(); it != distribution.end();)
  {
    if (matchedDataPoints.count(it->first) == 0)
      it = distribution.erase(it);
    else
      ++it;
  }
  CHECK(AreKeysEqual(distribution, matchedDataPoints),
        ("Mwms in |distribution| and in |matchedDataPoints| should have the same set of keys."));

  // Calculating how many points should have every mwm to keep the |distribution|.
  MwmToDataPoints const balancedDataPointNumber =
      BalancedDataPointNumber(move(distribution), move(matchedDataPoints), ignoreDataPointsNumber);
  uint64_t const totalBalancedDataPointsNumber = ValueSum(balancedDataPointNumber);

  // |balancedTable| is filled now with all the items from |tableCsvStream|.
  // Removing some items form |tableCsvStream| (if it's necessary) to correspond to
  // the distribution.
  FilterTable(balancedDataPointNumber, balancedTable);
  LOG(LINFO, ("Data points have balanced. Total distribution data points number:",
              totalDistributionDataPointsNumber,
              "Total matched data points number:", totalMatchedDataPointsNumber,
              "Total balanced data points number:", totalBalancedDataPointsNumber));
}
}  // namespace track_analyzing
