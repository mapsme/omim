#pragma once

#include "search/cancel_exception.hpp"
#include "search/geocoder.hpp"
#include "search/intermediate_result.hpp"
#include "search/keyword_lang_matcher.hpp"
#include "search/locality_finder.hpp"
#include "search/mode.hpp"
#include "search/result.hpp"
#include "search/reverse_geocoder.hpp"
#include "search/search_params.hpp"
#include "search/suggest.hpp"

#include "indexer/categories_holder.hpp"
#include "indexer/feature_decl.hpp"

#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"

#include "base/string_utils.hpp"

#include <set>
#include <string>
#include <utility>
#include <vector>

class CategoriesHolder;
class Index;

namespace storage
{
class CountryInfoGetter;
}  // namespace storage

namespace search
{
class VillagesCache;
class Emitter;
class PreResult2Maker;

class Ranker
{
public:
  struct Params
  {
    using TLocales = buffer_vector<int8_t, 3>;

    int8_t m_currentLocaleCode = CategoriesHolder::kEnglishCode;
    m2::RectD m_viewport;
    m2::PointD m_position;
    std::string m_pivotRegion;
    std::set<uint32_t> m_preferredTypes;
    bool m_suggestsEnabled = false;
    bool m_viewportSearch = false;

    std::string m_query;
    buffer_vector<strings::UniString, 32> m_tokens;
    // Prefix of the last token in the query.
    // We need it here to make suggestions.
    strings::UniString m_prefix;

    m2::PointD m_accuratePivotCenter = m2::PointD(0, 0);

    // A minimum distance between search results in meters, needed for
    // filtering of indentical search results.
    double m_minDistanceOnMapBetweenResults = 0.0;

    TLocales m_categoryLocales;

    size_t m_limit = 0;
  };

  static size_t const kBatchSize;

  Ranker(Index const & index, storage::CountryInfoGetter const & infoGetter, Emitter & emitter,
         CategoriesHolder const & categories, std::vector<Suggest> const & suggests,
         VillagesCache & villagesCache, my::Cancellable const & cancellable);
  virtual ~Ranker() = default;

  void Init(Params const & params, Geocoder::Params const & geocoderParams);

  bool IsResultExists(PreResult2 const & p, std::vector<IndexedValue> const & values);

  void MakePreResult2(Geocoder::Params const & params, std::vector<IndexedValue> & cont);

  Result MakeResult(PreResult2 const & r) const;
  void MakeResultHighlight(Result & res) const;

  void GetSuggestion(std::string const & name, std::string & suggest) const;
  void SuggestStrings();
  void MatchForSuggestions(strings::UniString const & token, int8_t locale, std::string const & prolog);
  void GetBestMatchName(FeatureType const & f, std::string & name) const;
  void ProcessSuggestions(std::vector<IndexedValue> & vec) const;

  virtual void SetPreResults1(std::vector<PreResult1> && preResults1) { m_preResults1 = std::move(preResults1); }
  virtual void UpdateResults(bool lastUpdate);

  void ClearCaches();

  inline void SetLocalityFinderLanguage(int8_t code) { m_localities.SetLanguage(code); }

  inline void SetLanguage(std::pair<int, int> const & ind, int8_t lang)
  {
    m_keywordsScorer.SetLanguage(ind, lang);
  }

  inline int8_t GetLanguage(std::pair<int, int> const & ind) const
  {
    return m_keywordsScorer.GetLanguage(ind);
  }

  inline void SetLanguages(std::vector<std::vector<int8_t>> const & languagePriorities)
  {
    m_keywordsScorer.SetLanguages(languagePriorities);
  }

  inline void SetKeywords(KeywordMatcher::StringT const * keywords, size_t count,
                          KeywordMatcher::StringT const & prefix)
  {
    m_keywordsScorer.SetKeywords(keywords, count, prefix);
  }

  inline void BailIfCancelled() { ::search::BailIfCancelled(m_cancellable); }

private:
  friend class PreResult2Maker;

  Params m_params;
  Geocoder::Params m_geocoderParams;
  ReverseGeocoder const m_reverseGeocoder;
  my::Cancellable const & m_cancellable;
  KeywordLangMatcher m_keywordsScorer;

  mutable LocalityFinder m_localities;

  Index const & m_index;
  storage::CountryInfoGetter const & m_infoGetter;
  Emitter & m_emitter;
  CategoriesHolder const & m_categories;
  std::vector<Suggest> const & m_suggests;

  std::vector<PreResult1> m_preResults1;
  std::vector<IndexedValue> m_tentativeResults;
};
}  // namespace search
