#pragma once

#include "partners_api/ads_utils.hpp"

#include "storage/storage_defines.hpp"

#include "indexer/ftypes_mapping.hpp"

#include "geometry/point2d.hpp"

#include "base/macros.hpp"

#include <cstdint>
#include <initializer_list>
#include <string>

namespace feature
{
class TypesHolder;
}

namespace ads
{
class PoiContainerBase
{
public:
  virtual ~PoiContainerBase() = default;
  virtual std::string GetBanner(feature::TypesHolder const & types,
                                storage::CountryId const & countryId,
                                std::string const & userLanguage) const = 0;

private:
  virtual bool HasBanner(feature::TypesHolder const & types, storage::CountryId const & countryId,
                         std::string const & userLanguage) const = 0;
  virtual std::string GetBannerForOtherTypes() const = 0;
};

// Class which matches feature types and banner ids.
class PoiContainer : public PoiContainerBase,
                     public WithSupportedLanguages,
                     public WithSupportedCountries
{
public:
  PoiContainer();

  // PoiContainerBase overrides:
  std::string GetBanner(feature::TypesHolder const & types, storage::CountryId const & countryId,
                        std::string const & userLanguage) const override;

  std::string GetBannerForOtherTypesForTesting() const;
protected:
  void AppendEntry(std::initializer_list<std::initializer_list<char const *>> && types,
                   std::string const & id);
  void AppendExcludedTypes(std::initializer_list<std::initializer_list<char const *>> && types);

private:
  bool HasBanner(feature::TypesHolder const & types, storage::CountryId const & countryId,
                 std::string const & userLanguage) const override;
  std::string GetBannerForOtherTypes() const override;

  ftypes::HashMapMatcher<uint32_t, std::string> m_typesToBanners;
  ftypes::HashSetMatcher<uint32_t> m_excludedTypes;

  DISALLOW_COPY(PoiContainer);
};

class SearchContainerBase
{
public:
  SearchContainerBase() = default;
  virtual ~SearchContainerBase() = default;

  virtual std::string GetBanner() const = 0;

private:
  virtual bool HasBanner() const = 0;

  DISALLOW_COPY(SearchContainerBase);
};
}  // namespace ads
