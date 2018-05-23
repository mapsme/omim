#pragma once

#include "base/mem_trie.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"

#include "std/algorithm.hpp"
#include "std/deque.hpp"
#include "std/iostream.hpp"
#include "std/map.hpp"
#include "std/shared_ptr.hpp"
#include "std/string.hpp"
#include "std/unique_ptr.hpp"
#include "std/unordered_map.hpp"
#include "std/utility.hpp"
#include "std/vector.hpp"

class Reader;

class CategoriesHolder
{
public:
  struct Category
  {
    static constexpr uint8_t kEmptyPrefixLength = 10;

    struct Name
    {
      string m_name;
      /// This language/locale code is completely different from our built-in langs in multilang_utf8_string.cpp
      /// and is only used for mapping user's input language to our values in categories.txt file
      int8_t m_locale;
      uint8_t m_prefixLengthToSuggest;
    };

    deque<Name> m_synonyms;

    inline void Swap(Category & r)
    {
      m_synonyms.swap(r.m_synonyms);
    }
  };

  struct Mapping
  {
    char const * m_name;
    int8_t m_code;
  };

  using GroupTranslations = unordered_map<string, vector<Category::Name>>;

private:
  using String = strings::UniString;
  using Type2CategoryCont = multimap<uint32_t, shared_ptr<Category>>;
  using Trie = base::MemTrie<String, base::VectorValues<uint32_t>>;

  Type2CategoryCont m_type2cat;

  // Maps locale and category token to the list of corresponding types.
  // Locale is treated as a special symbol prepended to the token.
  Trie m_name2type;

  GroupTranslations m_groupTranslations;

public:
  static int8_t constexpr kEnglishCode = 1;
  static int8_t constexpr kUnsupportedLocaleCode = -1;
  static uint8_t constexpr kMaxSupportedLocaleIndex = 31;
  static vector<Mapping> const kLocaleMapping;

  // List of languages that are currently disabled in the application
  // because their translations are not yet complete.
  static vector<string> kDisabledLanguages;

  explicit CategoriesHolder(unique_ptr<Reader> && reader);
  void LoadFromStream(istream & s);

  template <class ToDo>
  void ForEachCategory(ToDo && toDo) const
  {
    for (auto const & p : m_type2cat)
      toDo(*p.second);
  }

  template <class ToDo>
  void ForEachTypeAndCategory(ToDo && toDo) const
  {
    for (auto const & it : m_type2cat)
      toDo(it.first, *it.second);
  }

  template <class ToDo>
  void ForEachName(ToDo && toDo) const
  {
    for (auto const & p : m_type2cat)
    {
      for (auto const & synonym : p.second->m_synonyms)
        toDo(synonym);
    }
  }

  template <class ToDo>
  void ForEachNameByType(uint32_t type, ToDo && toDo) const
  {
    auto it = m_type2cat.find(type);
    if (it == m_type2cat.end())
      return;
    for (auto const & name : it->second->m_synonyms)
      toDo(name);
  }

  template <class ToDo>
  void ForEachTypeByName(int8_t locale, String const & name, ToDo && toDo) const
  {
    auto const localePrefix = String(1, static_cast<strings::UniChar>(locale));
    m_name2type.ForEachInNode(localePrefix + name, forward<ToDo>(toDo));
  }

  inline GroupTranslations const & GetGroupTranslations() const { return m_groupTranslations; }

  /// Search name for type with preffered locale language.
  /// If no name for this language, return en name.
  /// @return false if no categories for type.
  bool GetNameByType(uint32_t type, int8_t locale, string & name) const;

  /// @returns raw classificator type if it's not localized in categories.txt.
  string GetReadableFeatureType(uint32_t type, int8_t locale) const;

  // Exposes the tries that map category tokens to types.
  Trie const & GetNameToTypesTrie() const { return m_name2type; }
  bool IsTypeExist(uint32_t type) const;

  inline void Swap(CategoriesHolder & r)
  {
    m_type2cat.swap(r.m_type2cat);
    std::swap(m_name2type, r.m_name2type);
  }

  // Converts any language |locale| from UI to the corresponding
  // internal integer code.
  static int8_t MapLocaleToInteger(string const & locale);

  // Returns corresponding string representation for an internal
  // integer |code|. Returns an empty string in case of invalid
  // |code|.
  static string MapIntegerToLocale(int8_t code);

private:
  void AddCategory(Category & cat, vector<uint32_t> & types);
  static bool ValidKeyToken(String const & s);
};

inline void swap(CategoriesHolder & a, CategoriesHolder & b)
{
  return a.Swap(b);
}

// Defined in categories_holder_loader.cpp.
CategoriesHolder const & GetDefaultCategories();
