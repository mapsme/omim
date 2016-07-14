#pragma once
#include "search/metadata.hpp"
#include "search/ranking_info.hpp"

#include "indexer/feature_decl.hpp"

#include "editor/yes_no_unknown.hpp"

#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"

#include "base/buffer_vector.hpp"

#include "std/string.hpp"

#include "map/place_data.hpp"

namespace search
{

// Search result. Search returns a list of them, ordered by score.
class Result
{
public:
  enum ResultType
  {
    RESULT_FEATURE,
    RESULT_LATLON,
    RESULT_SUGGEST_PURE,
    RESULT_SUGGEST_FROM_FEATURE
  };

  /// For RESULT_FEATURE.
  Result(FeatureID const & id, m2::PointD const & pt, string const & str, string const & address,
         string const & type, uint32_t featureType, Metadata const & meta = {});

  /// For RESULT_LATLON.
  Result(m2::PointD const & pt, string const & latlon, string const & address);

  /// For RESULT_SUGGESTION_PURE.
  Result(string const & str, string const & suggest);

  /// For RESULT_SUGGESTION_FROM_FEATURE.
  Result(Result const & res, string const & suggest);

  string const & GetString() const { return m_str; }
  string const & GetFeatureType() const { return m_type; }

  /// Formatted data that should be displayed in the GUI.
  place_data::Data const & GetPlaceData() const { return m_placeData; }
  /// Formatted address that should be displayed in the GUI.
  string const & GetAddress() const { return m_address; }

  bool IsSuggest() const;
  bool HasPoint() const;

  /// Type of the result.
  ResultType GetResultType() const;

  /// Feature id in mwm.
  /// @precondition GetResultType() == RESULT_FEATURE
  FeatureID const & GetFeatureID() const;

  /// Center point of a feature.
  /// @precondition HasPoint() == true
  m2::PointD GetFeatureCenter() const;

  /// String to write in the search box.
  /// @precondition IsSuggest() == true
  char const * GetSuggestionString() const;

  bool IsEqualSuggest(Result const & r) const;
  bool IsEqualFeature(Result const & r) const;

  void AddHighlightRange(pair<uint16_t, uint16_t> const & range);
  pair<uint16_t, uint16_t> const & GetHighlightRange(size_t idx) const;
  inline size_t GetHighlightRangesCount() const { return m_hightlightRanges.size(); }

  void AppendCity(string const & name);

  int32_t GetPositionInResults() const { return m_positionInResults; }
  void SetPositionInResults(int32_t pos) { m_positionInResults = pos; }

  inline RankingInfo const & GetRankingInfo() const { return m_info; }

  template <typename TInfo>
  inline void SetRankingInfo(TInfo && info)
  {
    m_info = forward<TInfo>(info);
  }

  // Returns a representation of this result that is
  // sent to the statistics servers and later used to measure
  // the quality of our search engine.
  string ToStringForStats() const;

private:
  FeatureID m_id;
  m2::PointD m_center;
  place_data::Data m_placeData;
  string m_str, m_address, m_type;
  uint32_t m_featureType;
  string m_suggestionStr;
  buffer_vector<pair<uint16_t, uint16_t>, 4> m_hightlightRanges;

  RankingInfo m_info;

  // The position that this result occupied in the vector returned
  // by a search query. -1 if undefined.
  int32_t m_positionInResults = -1;
};

class Results
{
  vector<Result> m_vec;

  enum StatusT
  {
    NONE,             // default status
    ENDED_CANCELLED,  // search ended with canceling
    ENDED             // search ended itself
  };
  StatusT m_status;

  explicit Results(bool isCancelled)
  {
    m_status = (isCancelled ? ENDED_CANCELLED : ENDED);
  }

public:
  Results() : m_status(NONE) {}

  /// @name To implement end of search notification.
  //@{
  static Results GetEndMarker(bool isCancelled) { return Results(isCancelled); }
  bool IsEndMarker() const { return (m_status != NONE); }
  bool IsEndedNormal() const { return (m_status == ENDED); }
  //@}

  bool AddResult(Result && res);
  /// Fast function that don't do any duplicates checks.
  /// Used in viewport search only.
  void AddResultNoChecks(Result && res)
  {
    ASSERT_LESS(m_vec.size(), numeric_limits<int32_t>::max(), ());
    res.SetPositionInResults(static_cast<int32_t>(m_vec.size()));
    m_vec.push_back(move(res));
  }

  inline void Clear() { m_vec.clear(); }

  typedef vector<Result>::const_iterator IterT;
  inline IterT Begin() const { return m_vec.begin(); }
  inline IterT End() const { return m_vec.end(); }

  inline size_t GetCount() const { return m_vec.size(); }
  size_t GetSuggestsCount() const;

  inline Result & GetResult(size_t i)
  {
    ASSERT_LESS(i, m_vec.size(), ());
    return m_vec[i];
  }

  inline Result const & GetResult(size_t i) const
  {
    ASSERT_LESS(i, m_vec.size(), ());
    return m_vec[i];
  }

  inline void Swap(Results & rhs)
  {
    m_vec.swap(rhs.m_vec);
  }
};

struct AddressInfo
{
  string m_country, m_city, m_street, m_house, m_name;
  vector<string> m_types;
  double m_distanceMeters = -1.0;

  string GetPinName() const;    // Caroline
  string GetPinType() const;    // shop

  string FormatPinText() const; // Caroline (clothes shop)
  string FormatTypes() const;   // clothes shop
  string GetBestType() const;
  bool IsEmptyName() const;

  enum AddressType { DEFAULT, SEARCH_RESULT };
  // 7 vulica Frunze
  string FormatHouseAndStreet(AddressType type = DEFAULT) const;
  // 7 vulica Frunze, Minsk, Belarus
  string FormatAddress(AddressType type = DEFAULT) const;
  // Caroline, 7 vulica Frunze, Minsk, Belarus
  string FormatNameAndAddress(AddressType type = DEFAULT) const;

  friend string DebugPrint(AddressInfo const & info);

  void Clear();
};

string DebugPrint(search::Result const &);

}  // namespace search
