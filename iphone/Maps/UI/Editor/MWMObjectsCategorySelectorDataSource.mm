#import "MWMObjectsCategorySelectorDataSource.h"

#include "LocaleTranslator.h"

#include "Framework.h"

#include "indexer/classificator.hpp"

#include "platform/localization.hpp"

#include "editor/new_feature_categories.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace osm;

namespace
{
using Category = std::pair<std::string, NewFeatureCategories::TypeName>;
using Categories = std::vector<Category>;

std::string locale()
{
  return locale_translator::bcp47ToTwineLanguage(NSLocale.currentLocale.localeIdentifier);
}

void SortByTranslation(Categories & result)
{
  std::sort(result.begin(), result.end(), [](Category const & lhs, Category const & rhs)
  {
    return lhs.first < rhs.first;
  });
}
}  // namespace

@interface MWMObjectsCategorySelectorDataSource()
{
  NewFeatureCategories m_categories;
  Categories m_filteredCategories;
  Categories m_allCategories;
}

@end

@implementation MWMObjectsCategorySelectorDataSource

- (instancetype)init
{
  self = [super init];
  if (self)
    [self load];
    
  return self;
}

- (void)load
{
  m_categories = GetFramework().GetEditorCategories();
  m_categories.AddLanguage(locale());
  auto const & types = m_categories.GetAllCreatableTypeNames();
  
  m_allCategories.reserve(types.size());
  for (auto const & type : types)
    m_allCategories.emplace_back(platform::GetLocalizedTypeName(type), type);

  SortByTranslation(m_allCategories);
}

- (void)search:(NSString *)query
{
  m_filteredCategories.clear();
  
  if (query.length == 0)
    return;
  
  auto const types = m_categories.Search([query UTF8String]);
  
  m_filteredCategories.reserve(types.size());
  for (auto const & type : types)
    m_filteredCategories.emplace_back(platform::GetLocalizedTypeName(type), type);
  
  SortByTranslation(m_filteredCategories);
}

- (NSString *)getTranslation:(NSInteger)row
{
  return m_filteredCategories.empty()
      ? @(m_allCategories[row].first.c_str()) : @(m_filteredCategories[row].first.c_str());
}

- (NSString *)getType:(NSInteger)row
{
  return m_filteredCategories.empty()
      ? @(m_allCategories[row].second.c_str()) : @(m_filteredCategories[row].second.c_str());
}

- (NSInteger)getTypeIndex:(NSString *)type
{
  auto const it = find_if(m_allCategories.cbegin(), m_allCategories.cend(),
                          [type](Category const & item)
                          {
                            return type.UTF8String == item.second;
                          });
  
  NSAssert(it != m_allCategories.cend(), @"Incorrect category!");
  return distance(m_allCategories.cbegin(), it);
}

- (NSInteger)size
{
  return m_filteredCategories.empty() ? m_allCategories.size() : m_filteredCategories.size();
}

@end
