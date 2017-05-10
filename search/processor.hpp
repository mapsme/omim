#pragma once
#include "search/categories_cache.hpp"
#include "search/categories_set.hpp"
#include "search/emitter.hpp"
#include "search/geocoder.hpp"
#include "search/hotels_filter.hpp"
#include "search/mode.hpp"
#include "search/pre_ranker.hpp"
#include "search/rank_table_cache.hpp"
#include "search/ranker.hpp"
#include "search/search_params.hpp"
#include "search/search_trie.hpp"
#include "search/suggest.hpp"
#include "search/token_slice.hpp"

#include "indexer/ftypes_matcher.hpp"
#include "indexer/index.hpp"
#include "indexer/rank_table.hpp"
#include "indexer/string_slice.hpp"

#include "geometry/rect2d.hpp"

#include "base/buffer_vector.hpp"
#include "base/cancellable.hpp"
#include "base/limited_priority_queue.hpp"
#include "base/string_utils.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <memory>
#include <unordered_set>
#include <vector>

class FeatureType;
class CategoriesHolder;

namespace coding
{
class CompressedBitVector;
}

namespace storage
{
class CountryInfoGetter;
}

namespace search
{
struct Locality;
struct Region;

class DoFindLocality;
class FeatureLoader;
class Geocoder;
class HouseCompFactory;
class PreResult2Maker;  // todo(@m) merge with Ranker
class QueryParams;
class Ranker;
class ReverseGeocoder;

class Processor : public my::Cancellable
{
public:
  // Maximum result candidates count for each viewport/criteria.
  static size_t const kPreResultsCount;

  static double const kMinViewportRadiusM;
  static double const kMaxViewportRadiusM;

  Processor(Index const & index, CategoriesHolder const & categories,
            std::vector<Suggest> const & suggests, storage::CountryInfoGetter const & infoGetter);

  inline void SupportOldFormat(bool b) { m_supportOldFormat = b; }

  void Init(bool viewportSearch);

  /// @param[in]  forceUpdate Pass true (default) to recache feature's ids even
  /// if viewport is a part of the old cached rect.
  void SetViewport(m2::RectD const & viewport, bool forceUpdate);
  void SetPreferredLocale(std::string const & locale);
  void SetInputLocale(std::string const & locale);
  void SetQuery(std::string const & query);
  // TODO (@y): this function must be removed.
  void SetRankPivot(m2::PointD const & pivot);
  inline void SetMode(Mode mode) { m_mode = mode; }
  inline void SetSuggestsEnabled(bool enabled) { m_suggestsEnabled = enabled; }
  inline void SetPosition(m2::PointD const & position) { m_position = position; }
  inline void SetMinDistanceOnMapBetweenResults(double distance)
  {
    m_minDistanceOnMapBetweenResults = distance;
  }
  inline void SetOnResults(SearchParams::TOnResults const & onResults) { m_onResults = onResults; }
  inline std::string const & GetPivotRegion() const { return m_region; }
  inline m2::PointD const & GetPosition() const { return m_position; }

  /// Suggestions language code, not the same as we use in mwm data
  int8_t m_inputLocaleCode, m_currentLocaleCode;

  inline bool IsEmptyQuery() const { return (m_prefix.empty() && m_tokens.empty()); }
  void Search(SearchParams const & params, m2::RectD const & viewport);

  // Tries to generate a (lat, lon) result from |m_query|.
  void SearchCoordinates();

  void InitParams(QueryParams & params);

  void InitGeocoder(Geocoder::Params & params);
  void InitPreRanker(Geocoder::Params const & geocoderParams);
  void InitRanker(Geocoder::Params const & geocoderParams);
  void InitEmitter();

  void ClearCaches();

protected:
  enum ViewportID
  {
    DEFAULT_V = -1,
    CURRENT_V = 0,
    LOCALITY_V = 1,
    COUNT_V = 2  // Should always be the last
  };

  friend std::string DebugPrint(ViewportID viewportId);

  friend class BestNameFinder;
  friend class DoFindLocality;
  friend class FeatureLoader;
  friend class HouseCompFactory;
  friend class PreResult2Maker;
  friend class Ranker;

  using TMWMVector = std::vector<std::shared_ptr<MwmInfo>>;
  using TOffsetsVector = std::map<MwmSet::MwmId, std::vector<uint32_t>>;
  using TFHeader = feature::DataHeader;
  using TLocales = buffer_vector<int8_t, 3>;

  TLocales GetCategoryLocales() const;

  template <typename ToDo>
  void ForEachCategoryType(StringSliceBase const & slice, ToDo && toDo) const;

  template <typename ToDo>
  void ForEachCategoryTypeFuzzy(StringSliceBase const & slice, ToDo && toDo) const;

  m2::PointD GetPivotPoint() const;
  m2::RectD GetPivotRect() const;

  void SetViewportByIndex(m2::RectD const & viewport, size_t idx, bool forceUpdate);
  void ClearCache(size_t ind);

  CategoriesHolder const & m_categories;
  storage::CountryInfoGetter const & m_infoGetter;

  std::string m_region;
  std::string m_query;
  buffer_vector<strings::UniString, 32> m_tokens;
  strings::UniString m_prefix;
  set<uint32_t> m_preferredTypes;

  m2::RectD m_viewport[COUNT_V];
  m2::PointD m_pivot;
  m2::PointD m_position;
  double m_minDistanceOnMapBetweenResults;
  Mode m_mode;
  bool m_suggestsEnabled;
  std::shared_ptr<hotels_filter::Rule> m_hotelsFilter;
  SearchParams::TOnResults m_onResults;

  /// @name Get ranking params.
  //@{
  /// @return Rect for viewport-distance calculation.
  m2::RectD const & GetViewport(ViewportID vID = DEFAULT_V) const;
  //@}

  void SetLanguage(int id, int8_t lang);
  int8_t GetLanguage(int id) const;

  bool m_supportOldFormat;

protected:
  bool m_viewportSearch;

  VillagesCache m_villagesCache;

  Emitter m_emitter;
  Ranker m_ranker;
  PreRanker m_preRanker;
  Geocoder m_geocoder;
};
}  // namespace search
