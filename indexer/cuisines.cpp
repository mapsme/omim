#include "indexer/cuisines.hpp"

#include "base/stl_add.hpp"
#include "base/string_utils.hpp"

#include <utility>

namespace osm
{
// static
Cuisines & Cuisines::Instance()
{
  static Cuisines instance;
  return instance;
}

namespace
{
void InitializeCuisinesForLocale(platform::TGetTextByIdPtr & ptr, std::string const & lang)
{
  if (!ptr || ptr->GetLocale() != lang)
    ptr = GetTextByIdFactory(platform::TextSource::Cuisines, lang);
  CHECK(ptr, ("Error loading cuisines translations for", lang, "language."));
}

std::string TranslateImpl(platform::TGetTextByIdPtr const & ptr, std::string const & key)
{
  ASSERT(ptr, ("ptr should be initialized before calling this function."));
  return ptr->operator()(key);
}
}  // namespace

void Cuisines::Parse(std::string const & osmRawCuisinesTagValue, std::vector<std::string> & outCuisines)
{
  strings::Tokenize(osmRawCuisinesTagValue, ";", MakeBackInsertFunctor(outCuisines));
}

void Cuisines::ParseAndLocalize(std::string const & osmRawCuisinesTagValue, std::vector<std::string> & outCuisines,
                                std::string const & lang)
{
  Parse(osmRawCuisinesTagValue, outCuisines);
  InitializeCuisinesForLocale(m_translations, lang);
  for (auto & cuisine : outCuisines)
  {
    std::string tr = TranslateImpl(m_translations, cuisine);
    if (!tr.empty())
      cuisine = std::move(tr);
  }
}

std::string Cuisines::Translate(std::string const & singleOsmCuisine, std::string const & lang)
{
  ASSERT(singleOsmCuisine.find(';') == std::string::npos,
         ("Please call Parse method for raw OSM cuisine string."));
  InitializeCuisinesForLocale(m_translations, lang);
  return TranslateImpl(m_translations, singleOsmCuisine);
}

TAllCuisines Cuisines::AllSupportedCuisines() { return m_translations->GetAllSortedTranslations(); }
}  // namespace osm
