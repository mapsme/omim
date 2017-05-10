#pragma once

#include "indexer/categories_holder.hpp"

#include <string>
#include <vector>

namespace search
{
class DisplayedCategories
{
public:
  DisplayedCategories(CategoriesHolder const & holder);

  // Returns a list of English names of displayed categories for the
  // categories search tab. It's guaranteed that the list remains the
  // same during the application lifetime, keys may be used as parts
  // of resources ids.
  static std::vector<std::string> const & GetKeys();

  // Calls |fn| on each pair (synonym name, synonym locale) for the
  // |key|.
  template <typename Fn>
  void ForEachSynonym(std::string const & key, Fn && fn) const
  {
    auto const & translations = m_holder.GetGroupTranslations();
    auto const it = translations.find("@" + key);
    if (it == translations.end())
      return;

    for (auto const & name : it->second)
      fn(name.m_name, CategoriesHolder::MapIntegerToLocale(name.m_locale));
  }

 private:
  CategoriesHolder const & m_holder;
};
}  // namespace search
