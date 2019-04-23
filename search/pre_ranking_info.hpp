#pragma once

#include "search/intersection_result.hpp"
#include "search/model.hpp"
#include "search/token_range.hpp"

#include "indexer/feature_decl.hpp"

#include "geometry/point2d.hpp"

#include "base/assert.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace search
{
struct PreRankingInfo
{
  PreRankingInfo(Model::Type type, TokenRange const & range)
  {
    ASSERT_LESS(type, Model::TYPE_COUNT, ());
    m_type = type;
    m_tokenRange[m_type] = range;
  }

  TokenRange const & InnermostTokenRange() const
  {
    ASSERT_LESS(m_type, Model::TYPE_COUNT, ());
    return m_tokenRange[m_type];
  }

  size_t GetNumTokens() const { return InnermostTokenRange().Size(); }

  // An abstract distance from the feature to the pivot.  Measurement
  // units do not matter here.
  double m_distanceToPivot = 0;

  m2::PointD m_center = m2::PointD::Zero();
  bool m_centerLoaded = false;

  // Tokens match to the feature name or house number.
  TokenRange m_tokenRange[Model::TYPE_COUNT];

  // Different geo-parts extracted from query.  Currently only poi,
  // building and street ids are in |m_geoParts|.
  IntersectionResult m_geoParts;

  // Id of the matched city, if any.
  FeatureID m_cityId;

  // True iff all tokens that are not stop-words
  // were used when retrieving the feature.
  bool m_allTokensUsed = true;

  // Rank of the feature.
  uint8_t m_rank = 0;

  // Popularity rank of the feature.
  uint8_t m_popularity = 0;

  // Confidence and UGC rating.
  // Confidence: 0 - no information
  //             1 - based on few reviews
  //             2 - based on average reviews number
  //             3 - based on large number of reviews.
  // Rating [4.0 ... 10.0]:
  //             4.0 and lower represented as 4.0
  //             higher ratings saved as is from UGC.
  std::pair<uint8_t, float> m_rating = {0, 0.0f};

  // Search type for the feature.
  Model::Type m_type = Model::TYPE_COUNT;
};

std::string DebugPrint(PreRankingInfo const & info);
}  // namespace search
