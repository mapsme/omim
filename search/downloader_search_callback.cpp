#include "search/downloader_search_callback.hpp"

#include "search/result.hpp"

#include "storage/country_info_getter.hpp"
#include "storage/storage.hpp"

#include "editor/editable_data_source.hpp"

#include "indexer/data_source.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include <string>
#include <utility>

namespace
{
bool GetGroupCountryId(storage::Storage const & storage, std::string & name)
{
  auto const & synonyms = storage.GetCountryNameSynonyms();

  if (storage.IsInnerNode(name))
    return true;
  auto const it = synonyms.find(name);
  if (it == synonyms.end())
    return false;
  if (!storage.IsInnerNode(it->second))
    return false;
  name = it->second;
  return true;
}

bool GetGroupCountryIdFromFeature(storage::Storage const & storage, FeatureType & ft,
                                  std::string & name)
{
  int8_t const langIndices[] = {StringUtf8Multilang::kEnglishCode,
                                StringUtf8Multilang::kDefaultCode,
                                StringUtf8Multilang::kInternationalCode};

  for (auto const langIndex : langIndices)
  {
    if (!ft.GetName(langIndex, name))
      continue;
    if (GetGroupCountryId(storage, name))
      return true;
  }
  return false;
}
}  // namespace

namespace search
{
DownloaderSearchCallback::DownloaderSearchCallback(Delegate & delegate,
                                                   DataSource const & dataSource,
                                                   storage::CountryInfoGetter const & infoGetter,
                                                   storage::Storage const & storage,
                                                   storage::DownloaderSearchParams params)
  : m_delegate(delegate)
  , m_dataSource(dataSource)
  , m_infoGetter(infoGetter)
  , m_storage(storage)
  , m_params(std::move(params))
{
}

void DownloaderSearchCallback::operator()(search::Results const & results)
{
  storage::DownloaderSearchResults downloaderSearchResults;
  std::set<storage::DownloaderSearchResult> uniqueResults;

  for (auto const & result : results)
  {
    if (result.GetResultType() == search::Result::Type::DownloaderEntry)
    {
      std::string groupFeatureName = result.GetCountryId();
      if (!GetGroupCountryId(m_storage, groupFeatureName))
        continue;

      storage::DownloaderSearchResult downloaderResult(groupFeatureName,
                                                       result.GetString() /* m_matchedName */);
      if (uniqueResults.find(downloaderResult) == uniqueResults.end())
      {
        uniqueResults.insert(downloaderResult);
        downloaderSearchResults.m_results.push_back(downloaderResult);
      }
      continue;
    }

    if (!result.HasPoint())
      continue;

    if (result.GetResultType() != search::Result::Type::LatLon)
    {
      FeatureID const & fid = result.GetFeatureID();
      FeaturesLoaderGuard loader(m_dataSource, fid.m_mwmId);
      auto ft = loader.GetFeatureByIndex(fid.m_index);
      if (!ft)
      {
        LOG(LERROR, ("Feature can't be loaded:", fid));
        continue;
      }

      ftypes::LocalityType const type = ftypes::IsLocalityChecker::Instance().GetType(*ft);

      if (type == ftypes::LocalityType::Country || type == ftypes::LocalityType::State)
      {
        std::string groupFeatureName;
        if (GetGroupCountryIdFromFeature(m_storage, *ft, groupFeatureName))
        {
          storage::DownloaderSearchResult downloaderResult(groupFeatureName,
                                                           result.GetString() /* m_matchedName */);
          if (uniqueResults.find(downloaderResult) == uniqueResults.end())
          {
            uniqueResults.insert(downloaderResult);
            downloaderSearchResults.m_results.push_back(downloaderResult);
          }
          continue;
        }
      }
    }

    if (result.GetResultType() == search::Result::Type::LatLon)
    {
      auto const & mercator = result.GetFeatureCenter();
      storage::CountryId const & countryId = m_infoGetter.GetRegionCountryId(mercator);
      if (countryId == storage::kInvalidCountryId)
        continue;

      storage::DownloaderSearchResult downloaderResult(countryId,
                                                       result.GetString() /* m_matchedName */);
      if (uniqueResults.find(downloaderResult) == uniqueResults.end())
      {
        uniqueResults.insert(downloaderResult);
        downloaderSearchResults.m_results.push_back(downloaderResult);
      }
      continue;
    }
  }

  downloaderSearchResults.m_endMarker = results.IsEndMarker();

  if (m_params.m_onResults)
  {
    auto onResults = m_params.m_onResults;
    m_delegate.RunUITask(
        [onResults, downloaderSearchResults]() { onResults(downloaderSearchResults); });
  }
}
}  // namespace search
