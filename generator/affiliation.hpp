#pragma once

#include "generator/borders.hpp"
#include "generator/cells_merger.hpp"
#include "generator/factory_utils.hpp"
#include "generator/feature_builder.hpp"

#include <boost/geometry/index/rtree.hpp>
#include <optional>
#include <string>
#include <vector>

namespace feature
{
using CountryPolygonsPtr = borders::CountryPolygons const *;

class AffiliationInterface
{
public:
  virtual ~AffiliationInterface() = default;

  // The method will return the names of the buckets to which the fb belongs.
  virtual std::vector<CountryPolygonsPtr> GetAffiliations(FeatureBuilder const & fb) const = 0;
  virtual std::vector<CountryPolygonsPtr> GetAffiliations(m2::RectD const & rect) const = 0;
  virtual std::vector<CountryPolygonsPtr> GetAffiliations(m2::PointD const & point) const = 0;

  virtual bool HasCountryByName(std::string const & name) const = 0;

};

class CountriesFilesAffiliation : public AffiliationInterface
{
public:
  CountriesFilesAffiliation(std::string const & borderPath, bool haveBordersForWholeWorld);

  // AffiliationInterface overrides:
  std::vector<CountryPolygonsPtr> GetAffiliations(FeatureBuilder const & fb) const override;
  std::vector<CountryPolygonsPtr> GetAffiliations(m2::RectD const & rect) const override;
  std::vector<CountryPolygonsPtr> GetAffiliations(m2::PointD const & point) const override;

  bool HasCountryByName(std::string const & name) const override;

protected:
  borders::CountryPolygonsCollection const & m_countryPolygonsTree;
  std::string m_borderPath;
  bool m_haveBordersForWholeWorld;
};

class CountriesFilesIndexAffiliation : public CountriesFilesAffiliation
{
public:
  using Box = boost::geometry::model::box<m2::PointD>;
  using Value = std::pair<Box, std::vector<CountryPolygonsPtr>>;
  using Tree = boost::geometry::index::rtree<Value, boost::geometry::index::quadratic<16>>;

  CountriesFilesIndexAffiliation(std::string const & borderPath, bool haveBordersForWholeWorld);

  // AffiliationInterface overrides:
  std::vector<CountryPolygonsPtr> GetAffiliations(FeatureBuilder const & fb) const override;
  std::vector<CountryPolygonsPtr> GetAffiliations(m2::RectD const & rect) const override;
  std::vector<CountryPolygonsPtr> GetAffiliations(m2::PointD const & point) const override;

private:
  CountriesFilesIndexAffiliation(std::string const & borderPath, bool haveBordersForWholeWorld,
                                 std::shared_ptr<Tree> const & tree);

  static std::unique_ptr<CountriesFilesIndexAffiliation> BuildIndex(
      std::string const & borderPath, bool haveBordersForWholeWorld, std::deque<m2::RectD> & net,
      std::unique_ptr<AffiliationInterface> affiliation);

  std::shared_ptr<Tree> m_index;
};

class SingleAffiliation : public AffiliationInterface
{
public:
  explicit SingleAffiliation(std::string const & name);

  // AffiliationInterface overrides:
  std::vector<CountryPolygonsPtr> GetAffiliations(FeatureBuilder const & fb) const override;
  std::vector<CountryPolygonsPtr> GetAffiliations(m2::RectD const & rect) const override;
  std::vector<CountryPolygonsPtr> GetAffiliations(m2::PointD const & point) const override;

  bool HasCountryByName(std::string const & name) const override;

private:
  borders::CountryPolygons m_polygon;
};

enum class AffiliationType
{
  CountriesOld,
  Countries,
  Single
};

template <typename T>
void OstreamAppend(std::ostream & o, T && t)
{
  o << t << ' ';
}

template <typename T, typename... Args>
void OstreamAppend(std::ostream & o, T && first, Args &&... args)
{
  OstreamAppend(o, std::forward<T>(first));
  OstreamAppend(o, std::forward<Args>(args)...);
}

template <class... Args>
std::shared_ptr<AffiliationInterface> GetOrCreateAffiliation(AffiliationType type, Args... args)
{
  std::stringstream s;
  OstreamAppend(s, args...);
  auto const key = s.str();

  static std::unordered_map<std::string, std::shared_ptr<AffiliationInterface>> cache;
  static std::mutex cacheMutex;

  std::lock_guard<std::mutex> _(cacheMutex);
  auto const it = cache.find(key);
  if (it != std::cend(cache))
    return it->second;

  std::shared_ptr<AffiliationInterface> a;
  switch (type)
  {
  case AffiliationType::CountriesOld:
    a = generator::create<CountriesFilesAffiliation>(std::forward<Args>(args)...);
    break;
  case AffiliationType::Countries:
    a = generator::create<CountriesFilesIndexAffiliation>(std::forward<Args>(args)...);
    break;
  case AffiliationType::Single:
    a = generator::create<SingleAffiliation>(std::forward<Args>(args)...);
    break;
  }

  CHECK(a, (key));
  auto const & inserted = cache.emplace(key, std::move(a));
  return inserted.first->second;
}
}  // namespace feature
