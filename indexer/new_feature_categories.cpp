#include "new_feature_categories.hpp"

#include "indexer/categories_holder.hpp"
#include "indexer/classificator.hpp"

#include "base/assert.hpp"
#include "base/stl_helpers.hpp"

#include <algorithm>

#include "3party/Alohalytics/src/alohalytics.h"

namespace osm
{
NewFeatureCategories::NewFeatureCategories(editor::EditorConfig const & config)
{
  // TODO(mgsergio): Load types user can create from XML file.
  // TODO: Not every editable type can be created by user.
  // TODO(mgsergio): Store in Settings:: recent history of created types and use them here.
  // Max history items count should be set in the config.
  Classificator const & cl = classif();
  for (auto const & classificatorType : config.GetTypesThatCanBeAdded())
  {
    uint32_t const type = cl.GetTypeByReadableObjectName(classificatorType);
    if (type == 0)
    {
      LOG(LWARNING, ("Unknown type in Editor's config:", classificatorType));
      continue;
    }
    m_types.push_back(type);
  }
}

NewFeatureCategories::NewFeatureCategories(NewFeatureCategories && other)
  : m_index(std::move(other.m_index))
  , m_types(std::move(other.m_types))
  , m_categoriesByLang(std::move(other.m_categoriesByLang))
{
}

void NewFeatureCategories::AddLanguage(std::string lang)
{
  auto langCode = CategoriesHolder::MapLocaleToInteger(lang);
  if (langCode == CategoriesHolder::kUnsupportedLocaleCode)
  {
    lang = "en";
    langCode = CategoriesHolder::kEnglishCode;
  }
  if (m_categoriesByLang.find(lang) != m_categoriesByLang.end())
    return;

  NewFeatureCategories::TNames names;
  names.reserve(m_types.size());
  for (auto const & type : m_types)
  {
    m_index.AddCategoryByTypeAndLang(type, langCode);
    names.emplace_back(m_index.GetCategoriesHolder()->GetReadableFeatureType(type, langCode), type);
  }
  my::SortUnique(names);
  m_categoriesByLang[lang] = names;
}

NewFeatureCategories::TNames NewFeatureCategories::Search(std::string const & query, std::string lang) const
{
  auto langCode = CategoriesHolder::MapLocaleToInteger(lang);
  if (langCode == CategoriesHolder::kUnsupportedLocaleCode)
  {
    lang = "en";
    langCode = CategoriesHolder::kEnglishCode;
  }
  std::vector<uint32_t> resultTypes;
  m_index.GetAssociatedTypes(query, resultTypes);

  NewFeatureCategories::TNames result(resultTypes.size());
  for (size_t i = 0; i < result.size(); ++i)
  {
    result[i].first =
        m_index.GetCategoriesHolder()->GetReadableFeatureType(resultTypes[i], langCode);
    result[i].second = resultTypes[i];
  }
  my::SortUnique(result);

  alohalytics::TStringMap const stats = {
      {"query", query}, {"lang", lang}};
  alohalytics::LogEvent("searchNewFeatureCategory", stats);

  return result;
}

NewFeatureCategories::TNames const & NewFeatureCategories::GetAllCategoryNames(
    std::string const & lang) const
{
  auto it = m_categoriesByLang.find(lang);
  if (it == m_categoriesByLang.end())
    it = m_categoriesByLang.find("en");
  CHECK(it != m_categoriesByLang.end(), ());
  return it->second;
}
}  // namespace osm
